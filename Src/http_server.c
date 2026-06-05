#include "http_server.h"
#include "auto_tune.h"
#include "pid_controller.h"
#include "safety_monitor.h"
#include <stdio.h>
#include <string.h>

static struct netconn *http_conn = NULL;
static struct netconn *http_new_conn = NULL;
static err_t http_err;

/* ==================== Embedded Web UI Assets ==================== */
#include "webui_data.c"

void HTTP_Server_Init(void) {
    /* lwIP and netconn already initialized by STM32 Ethernet HAL/lwIP init */
}

void HTTP_Server_Process(void) {
    struct netbuf *inbuf;
    char *buf;
    uint16_t buflen;
    err_t recv_err;

    http_conn = netconn_new(NETCONN_TCP);
    if (http_conn == NULL) return;

    err_t bind_err = netconn_bind(http_conn, IP_ADDR_ANY, HTTP_SERVER_PORT);
    if (bind_err != ERR_OK) {
        netconn_delete(http_conn);
        return;
    }

    netconn_listen(http_conn);

    while (1) {
        netconn_accept(http_conn, &http_new_conn);
        if (http_new_conn == NULL) continue;

        netconn_set_recvtimeout(http_new_conn, 1000);

        recv_err = netconn_recv(http_new_conn, &inbuf);
        if (recv_err == ERR_OK && inbuf != NULL) {
            netbuf_data(inbuf, (void **)&buf, &buflen);
            if (buflen > 0) {
                /* Parse HTTP request line: GET /uri HTTP/1.1 */
                char method[8] = {0};
                char uri[HTTP_MAX_URI_LEN] = {0};
                if (sscanf(buf, "%7s %127s", method, uri) >= 2) {
                    /* Route request */
                    if (strncmp(uri, HTTP_JSON_API_PATH, strlen(HTTP_JSON_API_PATH)) == 0) {
                        /* JSON API endpoint */
                        char response[HTTP_MAX_RESPONSE_LEN];
                        if (strstr(uri, "/control")) {
                            HTTP_Json_GetControl(response, sizeof(response));
                        } else if (strstr(uri, "/autotune")) {
                            uint8_t ch = 0, cmd = 0;
                            sscanf(uri, "/api/autotune?ch=%hhu&cmd=%hhu", &ch, &cmd);
                            HTTP_Json_AutoTuneCmd(ch, cmd, response, sizeof(response));
                        } else if (strstr(uri, "/config")) {
                            HTTP_Json_ConfigGet(response, sizeof(response));
                        } else {
                            HTTP_Json_GetData(response, sizeof(response));
                        }

                        char header[256];
                        snprintf(header, sizeof(header),
                                 "%s%s%s%sContent-Length: %u\r\n\r\n%s",
                                 HTTP_HEADER_OK, HTTP_CONTENT_JSON,
                                 HTTP_CORS_HEADER, HTTP_CONNECTION_CLOSE,
                                 (unsigned int)strlen(response), response);
                        netconn_write(http_new_conn, header, strlen(header), NETCONN_COPY);
                    }
                    else if (strcmp(uri, "/") == 0 || strcmp(uri, "/index.html") == 0) {
                        char header[256];
                        snprintf(header, sizeof(header),
                                 "%s%s%sContent-Length: %u\r\n\r\n",
                                 HTTP_HEADER_OK, HTTP_CONTENT_HTML,
                                 HTTP_CONNECTION_CLOSE,
                                 (unsigned int)strlen(webui_html));
                        netconn_write(http_new_conn, header, strlen(header), NETCONN_COPY);
                        netconn_write(http_new_conn, webui_html, strlen(webui_html), NETCONN_COPY);
                    }
                    else if (strcmp(uri, "/style.css") == 0) {
                        char header[256];
                        snprintf(header, sizeof(header),
                                 "%s%s%sContent-Length: %u\r\n\r\n",
                                 HTTP_HEADER_OK, HTTP_CONTENT_CSS,
                                 HTTP_CONNECTION_CLOSE,
                                 (unsigned int)strlen(webui_css));
                        netconn_write(http_new_conn, header, strlen(header), NETCONN_COPY);
                        netconn_write(http_new_conn, webui_css, strlen(webui_css), NETCONN_COPY);
                    }
                    else if (strcmp(uri, "/app.js") == 0) {
                        char header[256];
                        snprintf(header, sizeof(header),
                                 "%s%s%sContent-Length: %u\r\n\r\n",
                                 HTTP_HEADER_OK, HTTP_CONTENT_JS,
                                 HTTP_CONNECTION_CLOSE,
                                 (unsigned int)strlen(webui_js));
                        netconn_write(http_new_conn, header, strlen(header), NETCONN_COPY);
                        netconn_write(http_new_conn, webui_js, strlen(webui_js), NETCONN_COPY);
                    }
                    else {
                        const char *not_found = HTTP_HEADER_NOT_FOUND HTTP_CONNECTION_CLOSE "\r\n";
                        netconn_write(http_new_conn, not_found, strlen(not_found), NETCONN_COPY);
                    }
                }
            }
            netbuf_delete(inbuf);
        }

        netconn_close(http_new_conn);
        netconn_delete(http_new_conn);
        http_new_conn = NULL;
    }
}

/* ==================== JSON API Handlers ==================== */
void HTTP_Json_GetData(char *buffer, uint16_t max_len) {
    char sensors_json[1024] = {0};
    char pid_json[1024]    = {0};
    char outputs_json[512] = {0};
    char faults_json[512]  = {0};

    /* Build sensors array */
    {
        strcat(sensors_json, "\"sensors\":[");
        for (uint8_t i = 0; i < NUM_SENSOR_CHANNELS; i++) {
            char entry[80];
            snprintf(entry, sizeof(entry),
                     "{\"ch\":%d,\"temp\":%.2f,\"cj\":%.2f,\"fault\":%d,\"valid\":%s}%s",
                     i + 1,
                     g_sys.sensors[i].temperature,
                     g_sys.sensors[i].cold_junction,
                     (int)g_sys.sensors[i].fault_flags,
                     g_sys.sensors[i].data_valid ? "true" : "false",
                     (i < NUM_SENSOR_CHANNELS - 1) ? "," : "");
            strcat(sensors_json, entry);
        }
        strcat(sensors_json, "]");
    }

    /* Build PID channels array */
    {
        strcat(pid_json, "\"pid_channels\":[");
        for (uint8_t i = 0; i < NUM_PID_CHANNELS; i++) {
            char entry[200];
            PIDChannel_t *p = &g_sys.pid_channels[i];
            snprintf(entry, sizeof(entry),
                     "{\"ch\":%d,\"sp\":%.2f,\"output\":%.2f,\"kp\":%.4f,\"ki\":%.4f,\"kd\":%.4f,"
                     "\"mode\":\"%s\",\"sensor_idx\":%d}%s",
                     i + 1, p->setpoint, p->output, p->kp, p->ki, p->kd,
                     p->mode == PID_MODE_AUTOMATIC ? "auto" : "manual",
                     p->sensor_index,
                     (i < NUM_PID_CHANNELS - 1) ? "," : "");
            strcat(pid_json, entry);
        }
        strcat(pid_json, "]");
    }

    /* Build output channels */
    {
        strcat(outputs_json, "\"outputs\":[");
        for (uint8_t i = 0; i < NUM_OUTPUT_CHANNELS; i++) {
            char entry[80];
            snprintf(entry, sizeof(entry),
                     "{\"ch\":%d,\"duty\":%.2f,\"enabled\":%s}%s",
                     i + 1, g_sys.outputs[i].duty_cycle,
                     g_sys.outputs[i].output_enabled ? "true" : "false",
                     (i < NUM_OUTPUT_CHANNELS - 1) ? "," : "");
            strcat(outputs_json, entry);
        }
        strcat(outputs_json, "]");
    }

    /* Auto-tune status */
    AutoTune_t *at = &g_sys.auto_tuner;
    const char *at_state_str = "idle";
    if (at->state == ATUNE_RUNNING) at_state_str = "running";
    else if (at->state == ATUNE_COMPLETED) at_state_str = "completed";
    else if (at->state == ATUNE_FAILED) at_state_str = "failed";

    /* Alarm flags */
    uint16_t alarms = SafetyMonitor_GetAlarmFlags();

    snprintf(buffer, max_len,
             "{%s,%s,%s,"
             "\"autotune\":{\"state\":\"%s\",\"channel\":%d},"
             "\"system\":{\"state\":%d,\"alarms\":%d,\"emergency\":%s},"
             "\"uptime_ms\":%lu}",
             sensors_json, pid_json, outputs_json,
             at_state_str, at->channel_index + 1,
             (int)g_sys.controller_state,
             (int)alarms,
             g_sys.emergency_stop ? "true" : "false",
             g_sys.system_ticks_ms);
}

void HTTP_Json_GetControl(char *buffer, uint16_t max_len) {
    /* Returns the control endpoints index */
    snprintf(buffer, max_len,
             "{\"endpoints\":[\"/api/data\",\"/api/autotune?ch=N&cmd=N\",\"/api/config\"],"
             "\"commands\":{\"autotune\":{\"0\":\"abort\",\"1\":\"start_zn_closed\",\"2\":\"start_cohen\"}}}");
}

void HTTP_Json_AutoTuneCmd(uint8_t channel, uint8_t cmd, char *buffer, uint16_t max_len) {
    if (channel == 0 || channel > NUM_PID_CHANNELS) {
        snprintf(buffer, max_len, "{\"error\":\"Invalid channel\"}");
        return;
    }

    switch (cmd) {
    case 0:
        AutoTune_Abort();
        snprintf(buffer, max_len, "{\"autotune\":\"aborted\",\"channel\":%d}", channel);
        break;
    case 1:
        AutoTune_Start(channel - 1, ATUNE_METHOD_ZN_CLOSED);
        snprintf(buffer, max_len, "{\"autotune\":\"started_zn_closed\",\"channel\":%d}", channel);
        break;
    case 2:
        AutoTune_Start(channel - 1, ATUNE_METHOD_COHEN);
        snprintf(buffer, max_len, "{\"autotune\":\"started_cohen\",\"channel\":%d}", channel);
        break;
    default:
        snprintf(buffer, max_len, "{\"error\":\"Invalid command\"}");
        break;
    }
}

void HTTP_Json_ConfigGet(char *buffer, uint16_t max_len) {
    snprintf(buffer, max_len,
             "{\"config\":{\"num_sensors\":%d,\"num_pid\":%d,\"num_outputs\":%d,"
             "\"sensor_type\":\"%s\",\"output_mode\":\"%s\","
             "\"pid_sample_ms\":%d,\"safety_max_temp\":%.1f,\"safety_min_temp\":%.1f}}",
             NUM_SENSOR_CHANNELS, NUM_PID_CHANNELS, NUM_OUTPUT_CHANNELS,
#if ACTIVE_SENSOR_TYPE == SENSOR_TYPE_MAX31855
             "MAX31855",
#else
             "MAX31865",
#endif
#if ACTIVE_OUTPUT_MODE == OUTPUT_MODE_AO
             "analog_0-10V",
#else
             "PWM",
#endif
             PID_SAMPLE_TIME_MS,
             MAX_SAFE_TEMPERATURE_C, MIN_SAFE_TEMPERATURE_C);
}
