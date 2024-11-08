#pragma once

#include <stdint.h>

#include <esp_err.h>
#include <esp_http_server.h>

#include <Wire.h>

#include "app.h"
#include "protocol_car_controls.hpp"
#include "protocol_android_controls.h"

extern httpd_uri_t g_uri_controls;

esp_err_t send_200(httpd_req_t *request);
esp_err_t send_400(httpd_req_t *request);
esp_err_t send_500(httpd_req_t *request);

char const* wire_status_to_string(uint8_t const status);

esp_err_t android_controls_handler(httpd_req_t *request);

esp_err_t i2c_message_arduino(size_t bytes, void *data);
esp_err_t i2c_message_arduino(NsControls::MessageTypeEspCam const command);
esp_err_t i2c_message_arduino(size_t bytes, void *data, NsControls::MessageTypeEspCam const command);

NsControls::MessageTypeArduino i2c_parse_message_arduino();
esp_err_t i2c_read_arduino(size_t const bytes, uint8_t *buffer);
esp_err_t i2c_await_arduino(int const bytes, size_t const interval_millis = 1, size_t const interval_count = 100);
NsControls::MessageTypeArduino i2c_await_message_arduino(size_t const interval_millis = 1, size_t const interval_count = 100);
