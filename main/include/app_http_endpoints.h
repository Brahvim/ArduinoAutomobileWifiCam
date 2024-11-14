#pragma once

#include <esp_err.h>
#include <esp_http_server.h>

extern httpd_uri_t g_uri_stream;
extern httpd_uri_t g_uri_controls;
extern int volatile g_car_steer_new;
extern int volatile g_car_steer_old;

esp_err_t send200(httpd_req_t *const request);
esp_err_t send400(httpd_req_t *const request);
esp_err_t send500(httpd_req_t *const request);

esp_err_t uri_handler_stream(httpd_req_t *const request);
esp_err_t uri_handler_controls(httpd_req_t *const request);
