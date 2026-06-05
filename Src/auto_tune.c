#include "auto_tune.h"
#include "pid_controller.h"
#include "output_driver.h"

/* ==================== Ziegler-Nichols PID Tuning Rules ==================== */
/*  P-only:     Kp = 0.5 * Ku
    PI:         Kp = 0.45 * Ku,  Ki = Kp / (Pu / 1.2)
    PID:        Kp = 0.6 * Ku,   Ki = 2 * Kp / Pu,  Kd = Kp * Pu / 8    */

/* ==================== Cohen-Coon PID Tuning Rules ==================== */
/*  τ = time constant,  θ = dead time,  K = process gain
    PID:  Kc = (1.35/K) * (θ/τ)^-1 * (τ/θ + 0.185)
          τi = 2.5 * θ * (τ + 0.185*θ) / (τ + 0.611*θ)
          τd = 0.37 * θ * τ / (τ + 0.185*θ)
    Ki = Kc / τi,  Kd = Kc * τd
*/

void AutoTune_Init(void) {
    AutoTune_t *at = &g_sys.auto_tuner;
    at->state             = ATUNE_IDLE;
    at->channel_index     = 0;
    at->start_time_ms     = 0;
    at->last_cross_time_ms = 0;
    at->step_output       = ATUNE_STEP_OUTPUT_PCT;
    at->amplitude         = 0.0f;
    at->period            = 0.0f;
    at->half_cycle_count  = 0;
    at->peak_detected     = false;
    at->prev_pv           = 0.0f;
    at->peak_max          = -999.0f;
    at->peak_min          = 999.0f;
    at->ultimate_gain     = 0.0f;
    at->ultimate_period   = 0.0f;
    at->output_at_start   = 0.0f;
}

void AutoTune_Start(uint8_t channel_index, AutoTuneMethod_t method) {
    AutoTune_t *at = &g_sys.auto_tuner;

    /* Abort any in-progress tune */
    if (at->state == ATUNE_RUNNING) {
        AutoTune_Abort();
    }

    at->state         = ATUNE_RUNNING;
    at->channel_index = channel_index;
    at->start_time_ms = g_sys.system_ticks_ms;

    PIDChannel_t *pid = &g_sys.pid_channels[channel_index];

    /* Store original setpoint and switch to P-only (Ki=0, Kd=0) */
    at->setpoint_original = pid->setpoint;
    at->output_at_start   = pid->output;

    /* Reset auto-tune tracking variables */
    at->amplitude         = 0.0f;
    at->period            = 0.0f;
    at->half_cycle_count  = 0;
    at->peak_detected     = false;
    at->last_cross_time_ms = at->start_time_ms;
    at->prev_pv           = g_sys.sensors[pid->sensor_index].temperature;
    at->peak_max          = at->prev_pv;
    at->peak_min          = at->prev_pv;
    at->ultimate_gain     = 0.0f;
    at->ultimate_period   = 0.0f;

    if (method == ATUNE_METHOD_ZN_CLOSED) {
        /* Z-N Closed Loop: set PID to P-only with initial low gain */
        at->ultimate_gain = 1.0f;
        pid->kp = at->ultimate_gain;
        pid->ki = 0.0f;
        pid->kd = 0.0f;
        pid->integral = 0.0f;
        pid->mode = PID_MODE_AUTOMATIC;
    } else {
        /* Cohen-Coon Open Loop: output step bump, PID off */
        pid->mode = PID_MODE_MANUAL;
        at->step_output = ATUNE_STEP_OUTPUT_PCT;
        at->output_at_start = pid->output;
        Output_SetChannel(pid->output_index, at->step_output);
    }
}

void AutoTune_Abort(void) {
    AutoTune_t *at = &g_sys.auto_tuner;
    PIDChannel_t *pid = &g_sys.pid_channels[at->channel_index];

    /* Restore setpoint */
    pid->setpoint = at->setpoint_original;
    pid->mode = PID_MODE_MANUAL;
    Output_SetChannel(pid->output_index, at->output_at_start);

    at->state = ATUNE_FAILED;
}

AutoTuneState_t AutoTune_GetState(void) {
    return g_sys.auto_tuner.state;
}

void AutoTune_Process(float current_pv) {
    AutoTune_t *at = &g_sys.auto_tuner;

    if (at->state != ATUNE_RUNNING) return;

    /* Timeout check */
    if ((g_sys.system_ticks_ms - at->start_time_ms) > ATUNE_TUNE_TIMEOUT_MS) {
        at->state = ATUNE_FAILED;
        return;
    }

    /* Use Ziegler-Nichols closed-loop method by default */
    AutoTune_ZN_ClosedLoop(current_pv);
}

/* ==================== Ziegler-Nichols Closed-Loop (Ultimate Gain) Method ==================== */
void AutoTune_ZN_ClosedLoop(float pv) {
    AutoTune_t *at = &g_sys.auto_tuner;
    PIDChannel_t *pid = &g_sys.pid_channels[at->channel_index];

    float setpoint = at->setpoint_original;

    /* Detect setpoint crossings to measure oscillation period */
    bool crossing_up   = (at->prev_pv <= setpoint && pv > setpoint);
    bool crossing_down = (at->prev_pv >= setpoint && pv < setpoint);

    /* Track peaks */
    if (pv > at->peak_max) at->peak_max = pv;
    if (pv < at->peak_min) at->peak_min = pv;

    if (crossing_up || crossing_down) {
        uint32_t now = g_sys.system_ticks_ms;
        uint32_t dt  = now - at->last_cross_time_ms;

        if (dt > 100 && at->half_cycle_count > 0) {
            at->period += (float)(dt * 2); /* Full period = 2 half-cycles */
        }
        at->last_cross_time_ms = now;
        at->half_cycle_count++;
    }

    /* After each full cycle (2 half-cycles), increase gain */
    if (at->half_cycle_count >= 2 && (at->half_cycle_count % 2 == 0)) {
        float amplitude = (at->peak_max - at->peak_min) / 2.0f;

        /* Check if oscillation is sustained (amplitude > hysteresis) */
        if (amplitude > ATUNE_HYSTERESIS_C) {
            /* Sustained oscillation detected → record Ku and Pu */
            at->ultimate_period = at->period / (at->half_cycle_count / 2);
            at->ultimate_gain   = pid->kp;

            AutoTune_ApplyResults();
            return;
        }

        /* Increase gain and continue */
        at->ultimate_gain += 0.5f;
        pid->kp = at->ultimate_gain;

        /* Reset peak tracking */
        at->peak_max = pv;
        at->peak_min = pv;
        at->period   = 0.0f;
        at->half_cycle_count = 0;

        /* Safety: limit max gain */
        if (at->ultimate_gain > 50.0f) {
            at->state = ATUNE_FAILED;
            pid->mode = PID_MODE_MANUAL;
            Output_SetChannel(pid->output_index, 0.0f);
            return;
        }
    }

    /* Check max cycles */
    if (at->half_cycle_count > (ATUNE_MAX_CYCLES * 2)) {
        at->state = ATUNE_FAILED;
        return;
    }

    at->prev_pv = pv;
}

/* ==================== Cohen-Coon Open-Loop (Process Reaction Curve) Method ==================== */
void AutoTune_CohenCoon_OpenLoop(float pv) {
    AutoTune_t *at = &g_sys.auto_tuner;
    PIDChannel_t *pid = &g_sys.pid_channels[at->channel_index];

    (void)pid;

    static float pv_initial   = 0.0f;
    static uint32_t dead_time_start = 0;
    static float rise_63 = 0.0f;
    static uint32_t time_const_start = 0;
    static uint8_t cc_state = 0;
    static float process_gain = 0.0f;

    switch (cc_state) {
    case 0:
        /* Wait for PV to respond (dead time) */
        if (at->half_cycle_count == 0) {
            pv_initial = pv;
            dead_time_start = g_sys.system_ticks_ms;
            at->half_cycle_count = 1;
        }
        if (fabsf(pv - pv_initial) > 0.5f) {
            /* Response detected */
            uint32_t dead_time_ms = g_sys.system_ticks_ms - dead_time_start;
            at->amplitude = (float)dead_time_ms / 1000.0f; /* Dead time in seconds */
            rise_63 = pv_initial + 0.632f * (at->step_output * 0.5f + pv_initial);
            time_const_start = g_sys.system_ticks_ms;
            cc_state = 1;
        }
        break;

    case 1:
        /* Wait for 63.2% of final value (approximate time constant) */
        if (pv >= rise_63 || (g_sys.system_ticks_ms - time_const_start) > 300000) {
            uint32_t tau_ms = g_sys.system_ticks_ms - time_const_start;
            float tau = (float)tau_ms / 1000.0f;
            float dead_time = at->amplitude;

            /* Compute process gain */
            process_gain = (pv - pv_initial) / at->step_output;

            /* Calculate PID using Cohen-Coon */
            float kp, ki, kd;
            AutoTune_Calculate_CohenCoon_PID(process_gain, tau, dead_time, &kp, &ki, &kd);

            pid->kp = kp;
            pid->ki = ki;
            pid->kd = kd;

            pid->setpoint = at->setpoint_original;
            pid->mode = PID_MODE_MANUAL;
            Output_SetChannel(pid->output_index, at->output_at_start);

            at->state = ATUNE_COMPLETED;
        }
        break;

    default:
        at->state = ATUNE_FAILED;
        break;
    }
}

/* Ziegler-Nichols closed-loop PID calculation */
void AutoTune_Calculate_ZN_PID(float ultimate_gain, float ultimate_period, float *kp, float *ki, float *kd) {
    float Ku = ultimate_gain;
    float Pu = ultimate_period;

    *kp = 0.6f * Ku;
    *ki = 2.0f * (*kp) / Pu;
    *kd = (*kp) * Pu / 8.0f;

    /* Safety clamping */
    if (*kp > 100.0f) *kp = 100.0f;
    if (*ki > 10.0f)  *ki = 10.0f;
    if (*kd > 50.0f)  *kd = 50.0f;
}

/* Cohen-Coon open-loop PID calculation */
void AutoTune_Calculate_CohenCoon_PID(float process_gain, float time_constant, float dead_time,
                                       float *kp, float *ki, float *kd) {
    float K  = process_gain;
    float tau = time_constant;
    float theta = dead_time;

    if (K <= 0.001f || tau <= 0.0f) {
        *kp = 1.0f; *ki = 0.0f; *kd = 0.0f;
        return;
    }

    float ratio = theta / tau;

    /* Cohen-Coon PID tuning formulas */
    float Kc = (1.35f / K) * (1.0f / ratio) * (ratio + 0.185f);
    float Ti = 2.5f * theta * (tau + 0.185f * theta) / (tau + 0.611f * theta);
    float Td = 0.37f * theta * tau / (tau + 0.185f * theta);

    *kp = Kc;
    *ki = Kc / Ti;
    *kd = Kc * Td;

    /* Safety clamping */
    if (*kp > 100.0f) *kp = 100.0f;
    if (*ki > 10.0f)  *ki = 10.0f;
    if (*kd > 50.0f)  *kd = 50.0f;
    if (*kp < 0.0f) *kp = 0.0f;
}

/* Apply computed tuning results to the PID channel */
void AutoTune_ApplyResults(void) {
    AutoTune_t *at = &g_sys.auto_tuner;
    PIDChannel_t *pid = &g_sys.pid_channels[at->channel_index];

    float kp, ki, kd;
    AutoTune_Calculate_ZN_PID(at->ultimate_gain, at->ultimate_period, &kp, &ki, &kd);

    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;

    /* Restore original setpoint */
    pid->setpoint = at->setpoint_original;
    pid->mode = PID_MODE_AUTOMATIC;
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;

    at->state = ATUNE_COMPLETED;
}
