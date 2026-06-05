#include "output_driver.h"

/* ==================== DAC Handle ==================== */
static DAC_HandleTypeDef hdac;

/* ==================== PWM Timer Handles ==================== */
static TIM_HandleTypeDef htim_pwm;

/* ==================== PWM Timer Channels Lookup ==================== */
static const uint32_t pwm_channels[NUM_OUTPUT_CHANNELS] = {
    PWM_TIMER_CHANNEL_1,
    PWM_TIMER_CHANNEL_2,
    PWM_TIMER_CHANNEL_3,
    PWM_TIMER_CHANNEL_4
};

static const uint32_t dac_channels[NUM_OUTPUT_CHANNELS] = {
    DAC_CHANNEL_1,
    DAC_CHANNEL_2,
    DAC_CHANNEL_1,
    DAC_CHANNEL_2
};

void Output_Init(void) {
    for (uint8_t i = 0; i < NUM_OUTPUT_CHANNELS; i++) {
        g_sys.outputs[i].mode           = ACTIVE_OUTPUT_MODE;
        g_sys.outputs[i].duty_cycle     = 0.0f;
        g_sys.outputs[i].dac_value      = 0;
        g_sys.outputs[i].pwm_compare    = 0;
        g_sys.outputs[i].output_enabled = false;
    }

#if (ACTIVE_OUTPUT_MODE == OUTPUT_MODE_AO)
    /* Initialize DAC */
    __HAL_RCC_DAC_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Mode  = GPIO_MODE_ANALOG;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pin   = GPIO_PIN_4 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOA, &gpio);

    hdac.Instance = DAC_INSTANCE;
    HAL_DAC_Init(&hdac);

    DAC_ChannelConfTypeDef sConfig = {0};
    sConfig.DAC_Trigger      = DAC_TRIGGER_NONE;
    sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
    HAL_DAC_ConfigChannel(&hdac, &sConfig, dac_channels[0]);
    HAL_DAC_ConfigChannel(&hdac, &sConfig, dac_channels[1]);

    HAL_DAC_Start(&hdac, dac_channels[0]);
    HAL_DAC_Start(&hdac, dac_channels[1]);
    HAL_DAC_SetValue(&hdac, dac_channels[0], DAC_ALIGN_12B_R, 0);
    HAL_DAC_SetValue(&hdac, dac_channels[1], DAC_ALIGN_12B_R, 0);

#elif (ACTIVE_OUTPUT_MODE == OUTPUT_MODE_PWM_VAL)
    /* Initialize PWM timer */
    __HAL_RCC_TIM2_CLK_ENABLE();

    htim_pwm.Instance               = PWM_TIMER_INSTANCE;
    htim_pwm.Init.Prescaler         = 7;         /* 72MHz / 8 = 9MHz */
    htim_pwm.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim_pwm.Init.Period            = PWM_PERIOD; /* ARR = 9999 => 1kHz PWM */
    htim_pwm.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim_pwm.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim_pwm);

    /* Configure PWM output channels */
    TIM_OC_InitTypeDef sConfigOC = {0};
    sConfigOC.OCMode     = TIM_OCMODE_PWM1;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    sConfigOC.Pulse      = 0;

    for (uint8_t i = 0; i < NUM_OUTPUT_CHANNELS; i++) {
        HAL_TIM_PWM_ConfigChannel(&htim_pwm, &sConfigOC, pwm_channels[i]);
        HAL_TIM_PWM_Start(&htim_pwm, pwm_channels[i]);
    }

    /* Configure GPIO AF for TIM2_CH1..CH4 on PA0, PA1, PA2, PA3 (or relevant pins) */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef gpio_pwm = {0};
    gpio_pwm.Mode      = GPIO_MODE_AF_PP;
    gpio_pwm.Pull      = GPIO_PULLDOWN;
    gpio_pwm.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio_pwm.Alternate = GPIO_AF1_TIM2;
    gpio_pwm.Pin       = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
    HAL_GPIO_Init(GPIOA, &gpio_pwm);
#endif
}

OutputStatus_t Output_SetChannel(uint8_t channel_index, float duty_cycle_pct) {
    if (channel_index >= NUM_OUTPUT_CHANNELS) {
        return OUTPUT_ERR_PARAM;
    }

    if (duty_cycle_pct < 0.0f) duty_cycle_pct = 0.0f;
    if (duty_cycle_pct > 100.0f) duty_cycle_pct = 100.0f;

    OutputChannel_t *ch = &g_sys.outputs[channel_index];
    ch->duty_cycle = duty_cycle_pct;

    if (g_sys.emergency_stop) {
        ch->output_enabled = false;
        duty_cycle_pct = 0.0f;
    }

    if (!ch->output_enabled) {
        duty_cycle_pct = 0.0f;
    }

#if (ACTIVE_OUTPUT_MODE == OUTPUT_MODE_AO)
    ch->dac_value = (uint16_t)((duty_cycle_pct / 100.0f) * 4095.0f);
    HAL_DAC_SetValue(&hdac, dac_channels[channel_index], DAC_ALIGN_12B_R, ch->dac_value);
    HAL_DAC_Start(&hdac, dac_channels[channel_index]);

#elif (ACTIVE_OUTPUT_MODE == OUTPUT_MODE_PWM_VAL)
    ch->pwm_compare = (uint16_t)((duty_cycle_pct / 100.0f) * (float)(PWM_PERIOD + 1));
    if (ch->pwm_compare > PWM_PERIOD) ch->pwm_compare = PWM_PERIOD;
    __HAL_TIM_SET_COMPARE(&htim_pwm, pwm_channels[channel_index], ch->pwm_compare);
#endif

    return OUTPUT_OK;
}

void Output_SetMode(uint8_t channel_index, OutputMode_t mode) {
    if (channel_index < NUM_OUTPUT_CHANNELS) {
        g_sys.outputs[channel_index].mode = mode;
    }
}

void Output_Enable(uint8_t channel_index, bool enable) {
    if (channel_index < NUM_OUTPUT_CHANNELS) {
        g_sys.outputs[channel_index].output_enabled = enable;
        if (!enable) {
            Output_SetChannel(channel_index, 0.0f);
        }
    }
}

void Output_EmergencyStop(void) {
    g_sys.emergency_stop = true;
    for (uint8_t i = 0; i < NUM_OUTPUT_CHANNELS; i++) {
        g_sys.outputs[i].output_enabled = false;
        Output_SetChannel(i, 0.0f);
    }
}

void Output_ClearEmergency(void) {
    g_sys.emergency_stop = false;
}
