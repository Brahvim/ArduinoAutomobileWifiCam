#include <errno.h>
#include <string.h>

#include <esp_log.h>
#include <esp_mac.h>
#include <esp32-hal-i2c.h>

#include "app_controls.hpp"

using namespace NsControls;

static char const *TAG = __FILE__;

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

	ESP_LOGD(TAG, "`/controls` queried!");
	ESP_LOGD(TAG, "Query length `%zu`!", str_query_len);

	char *str_query = (char*) calloc(str_query_len, sizeof(char));

	if (str_query == NULL) {
		ESP_LOGE(TAG, "URL parsing failed due to `NULL` return from `malloc()`. 500.");
		send500(p_request);
		// No free 👍️
		return ESP_OK;
	}

	esp_err_t err_httpd_last_call;
	ifl((err_httpd_last_call = httpd_req_get_url_query_str(p_request, str_query, str_query_len)) != ESP_OK) {
		ESP_LOGE(TAG, "URL parsing failed! Reason: \"%s\". 500.", esp_err_to_name(err_httpd_last_call));
		send500(p_request);
		free(str_query);
		return ESP_OK;
	}

	ESP_LOGI(TAG, "Query `%s`.", str_query);

	char const *str_param_name;

	char param_value_gear[20]; // Actually needs only `2` - digit and `\0`.
	str_param_name = g_android_controls_http_parameters[ANDROID_CONTROL_GEAR];
	ifl((err_httpd_last_call = httpd_query_key_value(str_query, str_param_name, param_value_gear, sizeof(param_value_gear))) != ESP_OK) {
		ESP_LOGE(TAG, "Parameter `%s` not parsed. Reason: \"%s\". 400.", str_param_name, esp_err_to_name(err_httpd_last_call));
		send400(p_request);
	} else {
		errno = 0;
		char *strtol_end;
		long value = strtol(param_value_gear, &strtol_end, 10);

		ifu(errno == ERANGE) {
			ESP_LOGE(TAG, "Parameter `%s` not in range. `400`!", str_param_name);
			send400(p_request);
			return ESP_OK;
		}

		ifu(strtol_end == param_value_gear) {
			ESP_LOGE(TAG, "Parameter `%s` not in range. `400`!", str_param_name);
			send400(p_request);
			return ESP_OK;
		}

		switch (value) {

			case ANDROID_GEAR_BACKWARDS: {

				pinMode(15, OUTPUT);
				analogWrite(15, 0);
				send200(p_request);

			} break;

			case ANDROID_GEAR_FORWARDS: {

				pinMode(15, OUTPUT);
				analogWrite(15, 1000);
				send200(p_request);

			} break;

			case ANDROID_GEAR_NEUTRAL: {

				pinMode(15, OUTPUT);
				analogWrite(15, 512);
				send200(p_request);

			} break;

			default: { // `400`!
				ESP_LOGE(TAG, "Parameter `%s` not in range. `400`!", str_param_name);
				send400(p_request);
				return ESP_OK;
			} break;

		}

	}
	str_param_name = g_android_controls_http_parameters[ANDROID_CONTROL_STOP];
	ifl((err_httpd_last_call = httpd_query_key_value(str_query, str_param_name, NULL, 0)) == ESP_ERR_NOT_FOUND) {
		ESP_LOGE(TAG, "Parameter `%s` not parsed. Reason: \"%s\". 400.", str_param_name, esp_err_to_name(err_httpd_last_call));
		send400(p_request);
	} else {
	}

	char param_value_steer[20]; // Needs only `5`, `-`, three-digit number, `\0`. That's 5 `char`s.
	str_param_name = g_android_controls_http_parameters[ANDROID_CONTROL_STEER];
	ifl((err_httpd_last_call = httpd_query_key_value(str_query, str_param_name, param_value_steer, sizeof(param_value_steer))) != ESP_OK) {
		ESP_LOGE(TAG, "Parameter `%s` not parsed. Reason: \"%s\". 400.", str_param_name, esp_err_to_name(err_httpd_last_call));
		send400(p_request);
	} else {

	}

	// ESP_LOGD(TAG, "`/controls` query OK! Checking parsed data...");
	// ESP_LOGI(TAG, "Parameter `%s` parsed string `%s`.", g_android_controls_http_parameters[ANDROID_CONTROL_GEAR], param_value_gear);
	// ESP_LOGI(TAG, "Parameter `%s` parsed string `%s`.", g_android_controls_http_parameters[ANDROID_CONTROL_STOP], param_value_stop);
	// ESP_LOGI(TAG, "Parameter `%s` parsed string `%s`.", g_android_controls_http_parameters[ANDROID_CONTROL_STEER], param_value_steer);
	// ESP_LOGD(TAG, "`/controls` handler exited. Nothing to do!...");

	free(str_query);
	return ESP_OK;
}
