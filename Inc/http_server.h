#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "system_config.h"
#include "data_structures.h"

#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"

#define HTTP_MAX_URI_LEN            128
#define HTTP_MAX_RESPONSE_LEN       4096
#define HTTP_JSON_API_PATH          "/api/data"
#define HTTP_JSON_API_PATH_CTRL     "/api/control"
#define HTTP_JSON_API_PATH_TUNE     "/api/autotune"
#define HTTP_JSON_API_PATH_CONFIG   "/api/config"

#define HTTP_HEADER_OK              "HTTP/1.1 200 OK\r\n"
#define HTTP_HEADER_BAD_REQUEST     "HTTP/1.1 400 Bad Request\r\n"
#define HTTP_HEADER_NOT_FOUND       "HTTP/1.1 404 Not Found\r\n"
#define HTTP_HEADER_SERVER_ERROR    "HTTP/1.1 500 Internal Server Error\r\n"
#define HTTP_CONTENT_JSON           "Content-Type: application/json\r\n"
#define HTTP_CONTENT_HTML           "Content-Type: text/html\r\n"
#define HTTP_CONTENT_JS             "Content-Type: application/javascript\r\n"
#define HTTP_CONTENT_CSS            "Content-Type: text/css\r\n"
#define HTTP_CORS_HEADER            "Access-Control-Allow-Origin: *\r\n"
#define HTTP_CONNECTION_CLOSE       "Connection: close\r\n"

typedef enum {
    HTTP_OK            = 0,
    HTTP_ERR_MEM       = 1,
    HTTP_ERR_PARSE     = 2,
    HTTP_ERR_INTERNAL  = 3
} HttpStatus_t;

void         HTTP_Server_Init(void);
void         HTTP_Server_Process(void);
HttpStatus_t HTTP_HandleRequest(struct netconn *conn);
HttpStatus_t HTTP_ServeStaticFile(struct netconn *conn, const char *uri);
HttpStatus_t HTTP_ServeWebUI(struct netconn *conn);
void         HTTP_Json_GetData(char *buffer, uint16_t max_len);
void         HTTP_Json_GetControl(char *buffer, uint16_t max_len);
HttpStatus_t HTTP_ParseQueryParams(const char *uri, char *out_params, uint16_t max_len);
void         HTTP_Json_AutoTuneCmd(uint8_t channel, uint8_t cmd, char *buffer, uint16_t max_len);
void         HTTP_Json_ConfigGet(char *buffer, uint16_t max_len);

extern const char webui_html[];
extern const char webui_css[];
extern const char webui_js[];

#endif /* HTTP_SERVER_H */
