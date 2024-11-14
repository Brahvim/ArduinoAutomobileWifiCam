// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <esp_log.h>
#include <esp_timer.h>
#include <esp_camera.h>
#include <esp_http_server.h>

#include <sdkconfig.h>

#include <esp_adc/adc_oneshot.h>

#include "app.h"
#include "app_http_endpoints.h"
#include "protocol_car_controls.hpp"
#include "protocol_android_controls.hpp"

#define PART_BOUNDARY "123456789000000000000987654321"

// Globals!

httpd_uri_t g_stream_uri = {

		.uri = "/stream",
		.method = HTTP_GET,
		.handler = uri_handler_stream,
		.user_ctx = NULL,
#ifdef CONFIG_HTTPD_WS_SUPPORT
		.is_websocket = true,
		.handle_ws_control_frames = false,
		.supported_subprotocol = NULL
#endif

};

httpd_uri_t g_uri_controls = {

		.uri = "/controls",
		.method = HTTP_GET,
		.handler = uri_handler_controls,
		.user_ctx = NULL,

#ifdef CONFIG_HTTPD_WS_SUPPORT
		.is_websocket = true,
		.handle_ws_control_frames = false,
		.supported_subprotocol = NULL,
#endif

};

int volatile g_car_steer_new = 0;
int volatile g_car_steer_old = 0;

httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

// `static` stuff.
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %d.%06d\r\n\r\n";
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";

static const char *TAG = __FILE__;

static bool s_car_mode_is_controls = true;

void start_camera_server() {
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	// config.max_uri_handlers = 16;
	config.max_uri_handlers = 2;

	ESP_LOGI(TAG, "Starting web server on port: '%d'", config.server_port);
	if (httpd_start(&camera_httpd, &config) == ESP_OK) {
		httpd_register_uri_handler(camera_httpd, &g_uri_controls);
	}

	config.ctrl_port += 1;
	config.server_port += 1;
	ESP_LOGI(TAG, "Starting stream server on port: '%d'", config.server_port);

	if (httpd_start(&stream_httpd, &config) == ESP_OK) {
		httpd_register_uri_handler(stream_httpd, &g_stream_uri);
	}
}

// HTTP stuff.
esp_err_t send200(httpd_req_t *const p_request) {
	esp_err_t to_ret = ESP_OK;
	to_ret &= httpd_resp_set_status(p_request, "200 OK");
	to_ret &= httpd_resp_send(p_request, NULL, 0);
	return to_ret;
}

esp_err_t send400(httpd_req_t *const p_request) {
	esp_err_t to_ret = ESP_OK;
	to_ret &= httpd_resp_set_status(p_request, "400 Bad Request");
	to_ret &= httpd_resp_send(p_request, NULL, 0);
	return to_ret;
}

esp_err_t send500(httpd_req_t *const p_request) {
	esp_err_t to_ret = ESP_OK;
	to_ret &= httpd_resp_set_status(p_request, "500 Internal Server Error");
	to_ret &= httpd_resp_send(p_request, NULL, 0);
	return to_ret;
}

esp_err_t uri_handler_stream(httpd_req_t *const p_request) {
	static int64_t last_frame = 0;
	static char *part_buf[128];

	struct timeval timestamp;
	camera_fb_t *fb = NULL;
	esp_err_t err = ESP_OK;

	size_t jpg_buf_len = 0;
	uint8_t *jpg_buf = NULL;

	if (!last_frame) {
		last_frame = esp_timer_get_time();
	}

	err = httpd_resp_set_type(p_request, _STREAM_CONTENT_TYPE);
	if (err != ESP_OK) {
		return err;
	}

	httpd_resp_set_hdr(p_request, "Access-Control-Allow-Origin", "*");
	httpd_resp_set_hdr(p_request, "X-Framerate", "60");

	while (true) {
		fb = esp_camera_fb_get();

		if (!fb) {

			ESP_LOGE(TAG, "Camera capture failed!...");
			err = ESP_FAIL;

		} else {

			timestamp.tv_usec = fb->timestamp.tv_usec;
			timestamp.tv_sec = fb->timestamp.tv_sec;

			if (fb->format != PIXFORMAT_JPEG) {

				bool jpeg_converted = frame2jpg(fb, 80, &jpg_buf, &jpg_buf_len);
				esp_camera_fb_return(fb);
				fb = NULL;

				if (!jpeg_converted) {
					ESP_LOGE(TAG, "JPEG compression failed");
					err = ESP_FAIL;
				}

			} else {

				jpg_buf_len = fb->len;
				jpg_buf = fb->buf;

			}

		}

		if (err == ESP_OK) {
			err = httpd_resp_send_chunk(p_request, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
		}

		if (err == ESP_OK) {
			size_t const hlen = snprintf((char*) part_buf, 128, _STREAM_PART, jpg_buf_len, timestamp.tv_sec, timestamp.tv_usec);
			err = httpd_resp_send_chunk(p_request, (const char*) part_buf, hlen);
		}

		if (err == ESP_OK) {
			err = httpd_resp_send_chunk(p_request, (const char*) jpg_buf, jpg_buf_len);
		}

		if (fb) {

			esp_camera_fb_return(fb);
			fb = NULL;
			jpg_buf = NULL;

		} else if (jpg_buf) {

			free(jpg_buf);
			jpg_buf = NULL;

		}

		if (err != ESP_OK) {

			ESP_LOGE(TAG, "Send frame failed");
			break;

		}

	}

	return err;
}

esp_err_t uri_handler_controls(httpd_req_t *const p_request) {
	size_t const str_query_len = 1 + httpd_req_get_url_query_len(p_request);
	httpd_resp_set_type(p_request, "application/octet-stream");

	int status_code = 200;

	ESP_LOGD(TAG, "`/controls` queried!");
	ESP_LOGD(TAG, "Query length `%zu`!", str_query_len);

	char *str_query = (char*) calloc(str_query_len, sizeof(char));

	ifu(str_query == NULL) { // Buffer was never allocated.
		ESP_LOGE(TAG, "URL parsing failed due to `NULL` return from `malloc()`. 500.");
		send500(p_request);
		// No freeing 👍️!
		return ESP_OK;
	}

	esp_err_t err_httpd_last_call;
	ifl((err_httpd_last_call = httpd_req_get_url_query_str(p_request, str_query, str_query_len)) != ESP_OK) {
		ESP_LOGE(TAG, "URL parsing failed! Reason: \"%s\". 500.", esp_err_to_name(err_httpd_last_call));
		send500(p_request);
		return ESP_OK;
	}

	ESP_LOGI(TAG, "Query `%s` received! Parsing query...", str_query);

	char const *str_param_name;
	char param_value_steer[5]; // Needs only `5`, `-`, possibly a three-digit number, `\0`. That's 5 `char`s.
	char param_value_gear[2]; // Needs only `2`! A `char` and `\0`!
	char param_value_mode; // Literally empty.

	str_param_name = g_android_controls_http_parameters[ANDROID_CONTROL_STEER];
	ifl((err_httpd_last_call = httpd_query_key_value(str_query, str_param_name, param_value_steer, sizeof(param_value_steer))) != ESP_OK) { // `400` on error.

		// ESP_LOGW(TAG, "Parameter `%s` not parsed. Reason: \"%s\". 400.", (char*) str_param_name, (char*) esp_err_to_name(err_httpd_last_call));
		ESP_LOGW(TAG, "Parameter `steer` not parsed. Reason: \"%s\". 400.", (char*) esp_err_to_name(err_httpd_last_call));
		status_code = 400;

	} else do {

		ESP_LOGI(TAG, "Parameter `%s` parsed string `%s`.", g_android_controls_http_parameters[ANDROID_CONTROL_STEER], param_value_steer);

		errno = 0;
		char *strtol_end;
		long value = strtol(param_value_steer, &strtol_end, 10);

		ifu(value > 255) { // Most frequent mistake!

			ESP_LOGE(TAG, "Parameter `%s` not in range. `400`!", str_param_name);
			status_code = 400;
			// return ESP_OK;
			break;

		}

		ifu(value < 0) { // *Slightly less frequent mistake!*

			ESP_LOGE(TAG, "Parameter `%s` not in range. `400`!", str_param_name);
			status_code = 400;
			// return ESP_OK;
			break;

		}

		ifu(errno == ERANGE) { // Okay, what is this blunder?

			ESP_LOGE(TAG, "Parameter `%s` not in range. `400`!", str_param_name);
			status_code = 400;
			// return ESP_OK;
			break;

		}

		ifu(*strtol_end != '\0') { // You seriously decided that in the name of data you'd send *none?*

			ESP_LOGE(TAG, "Parameter `%s` not in range. `400`!", str_param_name);
			status_code = 400;
			// return ESP_OK;
			break;

		}

		// pinMode(PIN_CAR_ARDUINO_STEER, OUTPUT);
		// analogWrite(CAR_PIN_ANALOG_ESP_CAM_STEER, value);
		send200(p_request);
		ESP_LOGI(TAG, "Car should steer towards the *%s* now.", value < 128 ? "left" : "right");
		return ESP_OK;

	} while (false);

	str_param_name = g_android_controls_http_parameters[ANDROID_CONTROL_GEAR];
	ifl((err_httpd_last_call = httpd_query_key_value(str_query, str_param_name, param_value_gear, sizeof(param_value_gear))) != ESP_OK) { // `400` on error.

		// ESP_LOGW(TAG, "Parameter `%s` not parsed. Reason: \"%s\". 400.", (char*) str_param_name, (char*) esp_err_to_name(err_httpd_last_call));
		ESP_LOGW(TAG, "`Parameter `gear` not parsed. Reason: \"%s\" 400.", esp_err_to_name(err_httpd_last_call));
		status_code = 400;

	} else {

		// ESP_LOGI(TAG, "Parameter `%s` parsed string `%s`.", (char*) g_android_controls_http_parameters[ANDROID_CONTROL_GEAR], (char*) param_value_gear);
		ESP_LOGI(TAG, "`/controls?gear` parsed successfully.");

		switch (param_value_gear[0]) {

			case ANDROID_GEAR_BACKWARDS: {

				// digitalWrite(CAR_PIN_DIGITAL_ESP_CAM_1, LOW);
				// digitalWrite(CAR_PIN_DIGITAL_ESP_CAM_2, HIGH);
				send200(p_request);
				ESP_LOGI(TAG, "Car should move backwards now.");
				return ESP_OK;

			} break;

			case ANDROID_GEAR_FORWARDS: {

				// digitalWrite(CAR_PIN_DIGITAL_ESP_CAM_1, HIGH);
				// digitalWrite(CAR_PIN_DIGITAL_ESP_CAM_2, LOW);
				send200(p_request);
				ESP_LOGI(TAG, "Car should move forwards now.");
				return ESP_OK;

			} break;

			case ANDROID_GEAR_NEUTRAL: {

				// digitalWrite(CAR_PIN_DIGITAL_ESP_CAM_1, HIGH);
				// digitalWrite(CAR_PIN_DIGITAL_ESP_CAM_2, HIGH);
				send200(p_request);
				ESP_LOGI(TAG, "Car should stop now.");
				return ESP_OK;

			} break;

			default: { // `400`!

				// ESP_LOGE(TAG, "Parameter `%s` not in range. `400`!", (char*) str_param_name);
				ESP_LOGE(TAG, "Parameter `gear` not in range. `400`!");
				status_code = 400;

			} break;

		}

	}

	str_param_name = g_android_controls_http_parameters[ANDROID_CONTROL_MODE];
	ifl((err_httpd_last_call = httpd_query_key_value(str_query, str_param_name, &param_value_mode, sizeof(param_value_mode))) == ESP_OK) { // `400` if not found.

		ESP_LOGI(TAG, "Car should be changing modes...");

		if (s_car_mode_is_controls) {

			// digitalWrite(CAR_PIN_DIGITAL_ESP_CAM_1, LOW);
			// digitalWrite(CAR_PIN_DIGITAL_ESP_CAM_2, LOW);
			send200(p_request);

			s_car_mode_is_controls = false;
			ESP_LOGI(TAG, "Car should avoid obstacles now.");
			return ESP_OK;

		} else {

			// digitalWrite(CAR_PIN_DIGITAL_ESP_CAM_1, HIGH);
			// digitalWrite(CAR_PIN_DIGITAL_ESP_CAM_2, LOW);
			send200(p_request);

			s_car_mode_is_controls = true;
			ESP_LOGI(TAG, "Car should listen to controls now.");
			return ESP_OK;

		}

	} else { // `400`.

		// ESP_LOGW(TAG, "Parameter `mode` not parsed. Reason: \"%s\". 400.", (char*) esp_err_to_name(err_httpd_last_call));
		ESP_LOGW(TAG, "Parameter `mode` not parsed. Reason: \"%s\". 400.", esp_err_to_name(err_httpd_last_call));
		status_code = 400;

	}

	ifl(status_code != 200) {
		ESP_LOGE(TAG, "`/controls` handler exited. Nothing to do!...");
		send400(p_request);
	}

	return ESP_OK;
}
