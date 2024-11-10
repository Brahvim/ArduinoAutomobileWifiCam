#pragma once

#include <stdint.h>

#include <esp_err.h>
#include <esp_http_server.h>

#include <Wire.h>

#include "app.h"
#include "protocol_car_controls.hpp"
#include "protocol_android_controls.hpp"

extern httpd_uri_t g_uri_controls;

esp_err_t send_200(httpd_req_t *request);
esp_err_t send_400(httpd_req_t *request);
esp_err_t send_500(httpd_req_t *request);

esp_err_t android_controls_handler(httpd_req_t *request);
