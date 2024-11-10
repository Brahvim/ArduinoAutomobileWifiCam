#pragma once

#include <esp_err.h>
#include <esp_http_server.h>

extern httpd_uri_t g_uri_controls;
extern int volatile g_carSteerNewValue;
extern int volatile g_carSteerPreviousValue;

esp_err_t send_200(httpd_req_t *request);
esp_err_t send_400(httpd_req_t *request);
esp_err_t send_500(httpd_req_t *request);

esp_err_t android_controls_handler(httpd_req_t *request);
