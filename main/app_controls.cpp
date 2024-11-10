#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <esp_log.h>
#include <esp_mac.h>
#include <esp32-hal-i2c.h>

#include <driver/ledc.h>

#include <Arduino.h>

#include "app.h"
#include "app_controls.hpp"
#include "protocol_car_controls.hpp"
#include "protocol_android_controls.hpp"

httpd_uri_t g_uri_controls = {

		.uri = "/controls",
		.method = HTTP_GET,
		.handler = android_controls_handler,
		.user_ctx = NULL,

#ifdef CONFIG_HTTPD_WS_SUPPORT
		.is_websocket = true,
		.handle_ws_control_frames = false,
		.supported_subprotocol = NULL,
#endif

};

int volatile g_carSteerNewValue = 0;
int volatile g_carSteerPreviousValue = 0;

static char const *TAG = __FILE__;
static bool s_carModeControls = true;

// HTTP stuff.
esp_err_t send200(httpd_req_t *p_request) {
	esp_err_t to_ret = ESP_OK;
	to_ret &= httpd_resp_set_status(p_request, "200 OK");
	to_ret &= httpd_resp_send(p_request, NULL, 0);
	return to_ret;
}

esp_err_t send400(httpd_req_t *p_request) {
	esp_err_t to_ret = ESP_OK;
	to_ret &= httpd_resp_set_status(p_request, "400 Bad Request");
	to_ret &= httpd_resp_send(p_request, NULL, 0);
	return to_ret;
}

esp_err_t send500(httpd_req_t *p_request) {
	esp_err_t to_ret = ESP_OK;
	to_ret &= httpd_resp_set_status(p_request, "500 Internal Server Error");
	to_ret &= httpd_resp_send(p_request, NULL, 0);
	return to_ret;
}

esp_err_t android_controls_handler(httpd_req_t *p_request) {
	size_t const str_query_len = 1 + httpd_req_get_url_query_len(p_request);
	httpd_resp_set_type(p_request, "application/octet-stream");

	bool unsuccessful = true;

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
		send400(p_request);

	} else {

		ESP_LOGI(TAG, "Parameter `%s` parsed string `%s`.", g_android_controls_http_parameters[ANDROID_CONTROL_STEER], param_value_steer);

		errno = 0;
		char *strtol_end;
		long value = strtol(param_value_steer, &strtol_end, 10);

		ifu(value > 255) { // Most frequent mistake!

			ESP_LOGE(TAG, "Parameter `%s` not in range. `400`!", str_param_name);
			send400(p_request);
			return ESP_OK;

		}

		ifu(value < 0) { // *Slightly less frequent mistake!*

			ESP_LOGE(TAG, "Parameter `%s` not in range. `400`!", str_param_name);
			send400(p_request);
			return ESP_OK;

		}

		ifu(errno == ERANGE) { // Okay, what is this blunder?

			ESP_LOGE(TAG, "Parameter `%s` not in range. `400`!", str_param_name);
			send400(p_request);
			return ESP_OK;

		}

		ifu(*strtol_end != '\0') { // You seriously decided that in the name of data you'd send *none?*

			ESP_LOGE(TAG, "Parameter `%s` not in range. `400`!", str_param_name);
			send400(p_request);
			return ESP_OK;

		}

		// pinMode(PIN_CAR_ARDUINO_STEER, OUTPUT);
		analogWrite(PIN_CAR_ESP_CAM_STEER, value);
		// ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, (value * 255) / 1023, 0);
		// ledcWrite(GPIO_NUM_14, (value * 255) / 1023);
		send200(p_request);
		unsuccessful = false;
		ESP_LOGI(TAG, "Car should steer towards the *%s* now.", value < 128 ? "left" : "right");

	}

	str_param_name = g_android_controls_http_parameters[ANDROID_CONTROL_GEAR];
	ifl((err_httpd_last_call = httpd_query_key_value(str_query, str_param_name, param_value_gear, sizeof(param_value_gear))) != ESP_OK) { // `400` on error.

		// ESP_LOGW(TAG, "Parameter `%s` not parsed. Reason: \"%s\". 400.", (char*) str_param_name, (char*) esp_err_to_name(err_httpd_last_call));
		ESP_LOGW(TAG, "`Parameter `gear` not parsed. Reason: \"%s\" 400.", esp_err_to_name(err_httpd_last_call));
		send400(p_request);

	} else {

		// ESP_LOGI(TAG, "Parameter `%s` parsed string `%s`.", (char*) g_android_controls_http_parameters[ANDROID_CONTROL_GEAR], (char*) param_value_gear);
		ESP_LOGI(TAG, "`/controls?gear` parsed successfully.");

		switch (param_value_gear[0]) {

			case ANDROID_GEAR_BACKWARDS: {

				digitalWrite(PIN_CAR_ESP_CAM_1, LOW);
				digitalWrite(PIN_CAR_ESP_CAM_2, HIGH);
				send200(p_request);
				unsuccessful = false;
				ESP_LOGI(TAG, "Car should move backwards now.");

			} break;

			case ANDROID_GEAR_FORWARDS: {

				digitalWrite(PIN_CAR_ESP_CAM_1, HIGH);
				digitalWrite(PIN_CAR_ESP_CAM_2, LOW);
				send200(p_request);
				unsuccessful = false;
				ESP_LOGI(TAG, "Car should move forwards now.");

			} break;

			case ANDROID_GEAR_NEUTRAL: {

				digitalWrite(PIN_CAR_ESP_CAM_1, HIGH);
				digitalWrite(PIN_CAR_ESP_CAM_2, HIGH);
				send200(p_request);
				unsuccessful = false;
				ESP_LOGI(TAG, "Car should stop now.");

			} break;

			default: { // `400`!

				// ESP_LOGE(TAG, "Parameter `%s` not in range. `400`!", (char*) str_param_name);
				ESP_LOGE(TAG, "Parameter `gear` not in range. `400`!");
				send400(p_request);
				return ESP_OK;

			} break;

		}

	}

	str_param_name = g_android_controls_http_parameters[ANDROID_CONTROL_MODE];
	ifl((err_httpd_last_call = httpd_query_key_value(str_query, str_param_name, &param_value_mode, sizeof(param_value_mode))) == ESP_OK) { // `400` if not found.

		ESP_LOGI(TAG, "Car should be changing modes...");

		if (s_carModeControls) {

			digitalWrite(PIN_CAR_ESP_CAM_1, LOW);
			digitalWrite(PIN_CAR_ESP_CAM_2, LOW);
			send200(p_request);

			unsuccessful = false;
			s_carModeControls = false;
			ESP_LOGI(TAG, "Car should avoid obstacles now.");

		} else {

			digitalWrite(PIN_CAR_ESP_CAM_1, HIGH);
			digitalWrite(PIN_CAR_ESP_CAM_2, LOW);
			send200(p_request);

			unsuccessful = false;
			s_carModeControls = true;
			ESP_LOGI(TAG, "Car should listen to controls now.");

		}

	} else { // `400`.

		// ESP_LOGW(TAG, "Parameter `mode` not parsed. Reason: \"%s\". 400.", (char*) esp_err_to_name(err_httpd_last_call));
		ESP_LOGW(TAG, "Parameter `mode` not parsed. Reason: \"%s\". 400.", esp_err_to_name(err_httpd_last_call));
		send400(p_request);

	}

	ifu(unsuccessful) {
		ESP_LOGE(TAG, "`/controls` handler exited. Nothing to do!...");
		send400(p_request);
	}

	return ESP_OK;
}
