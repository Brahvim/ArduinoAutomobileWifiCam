#include <errno.h>
#include <string.h>

#include <esp_log.h>
#include <esp_mac.h>

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

// I2C stuff.
esp_err_t i2c_await_arduino(size_t const p_bytes, size_t const p_interval_millis = 10, size_t const p_interval_count = 100) {
	Wire.requestFrom(I2C_ADDR_ARDUINO, sizeof(MessageTypeArduino));

	for (size_t i = p_interval_count; i < 1 || Wire.available() > p_bytes; --i) {
		delay(p_interval_millis);
	}

	return ESP_OK;
}

char const* wire_status_to_string(uint8_t const p_status) {
	/*

	`Wire.cpp` line `432`:
	"""
	https://www.arduino.cc/reference/en/language/functions/communication/wire/endtransmission/
	endTransmission() returns:
	0: success.
	1: data too long to fit in transmit buffer.
	2: received NACK on transmit of address.
	3: received NACK on transmit of data.
	4: other error.
	5: timeout
	"""

	PS The URL has changed to [ https://docs.arduino.cc/language-reference/en/functions/communication/Wire/endTransmission/ ].

	*/

	switch (p_status) {

		case 0: { return "Success"; } break;
		case 1: { return "Data written too large to fit in transmit buffer"; } break;
		case 2: { return "Received NACK when transmitting I2C address"; } break;
		case 3: { return "Received NACK when transmitting I2C data"; } break;
		case 4: { return "Other error"; } break;
		case 5: { return "I2C timeout"; } break;

		default: { return "Unknown error"; } break;

	}
}

esp_err_t i2c_message_arduino(size_t p_bytes, uint8_t* p_data) {
	Wire.beginTransmission(I2C_ADDR_ARDUINO);
	size_t const bytes_sent = Wire.write(p_data, p_bytes);
	uint8_t const wire_status = Wire.endTransmission();

	ifu(bytes_sent != p_bytes) {
		ESP_LOGE(TAG, "Arduino `Wire`/I2C transmission didn't send all bytes for data transaction.");
		return ESP_FAIL;
	}

	ifu(!wire_status) {
		ESP_LOGE(TAG, "Arduino `Wire`/I2C data transmission failure: \"%s\".", wire_status_to_string(wire_status));
		return ESP_FAIL;
	}

	return ESP_OK;
}

esp_err_t i2c_message_arduino(MessageTypeEspCam const p_command) {
	static_assert(sizeof(MessageTypeEspCam) == sizeof(uint8_t), "`MessageTypeEspCam` is no longer a single byte. *`memcpy()1!1!!`* (Use below code!)");

	// ...So if it ain't a byte no mo', *use this:*
	// uint8_t resultMessageTypeData[sizeof(MessageTypeEspCam)];
	//
	// memcpy(resultMessageTypeData, &p_command, sizeof(MessageTypeEspCam));
	// Wire.beginTransmission(I2C_ADDR_ARDUINO);
	// size_t const bytes_sent = Wire.write(resultMessageTypeData, sizeof(MessageTypeEspCam));
	// uint8_t const wire_status = Wire.endTransmission();

	Wire.beginTransmission(I2C_ADDR_ARDUINO);
	size_t const bytes_sent = Wire.write((uint8_t*) &p_command, sizeof(MessageTypeEspCam));
	uint8_t const wire_status = Wire.endTransmission();

	ifu(bytes_sent != sizeof(MessageTypeEspCam)) {
		ESP_LOGE(TAG, "Arduino `Wire`/I2C transmission didn't send all bytes for message transaction.");
		return ESP_FAIL;
	}

	ifu(!wire_status) {
		ESP_LOGE(TAG, "Arduino `Wire`/I2C message transmission failure: \"%s\".", wire_status_to_string(wire_status));
		return ESP_FAIL;
	}

	return ESP_OK;
}

esp_err_t i2c_read_arduino(size_t const p_bytes, uint8_t *p_buffer) {
	for (int i = 0; i < p_bytes; ++i) {
		p_buffer[i] = Wire.read(); // Compiler won't unroll this :(
	}

	return ESP_OK;
}

esp_err_t send_200(httpd_req_t *p_request) {
	httpd_resp_set_status(p_request, "200 OK");
	httpd_resp_send(p_request, NULL, 0);
	return ESP_OK;
}

esp_err_t send_400(httpd_req_t *p_request) {
	httpd_resp_set_status(p_request, "400 Bad Request");
	httpd_resp_send(p_request, NULL, 0);
	return ESP_OK;
}

esp_err_t send_500(httpd_req_t *p_request) {
	httpd_resp_set_status(p_request, "500 Internal Server Error");
	httpd_resp_send(p_request, NULL, 0);
	return ESP_OK;
}

esp_err_t android_controls_handler(httpd_req_t *p_request) {
	size_t const str_len = httpd_req_get_url_query_len(p_request);
	httpd_resp_set_type(p_request, ANDROID_HTTP_CONTENT_TYPE);

	ESP_LOGI(TAG, "`/controls` queried!");

	ifu(str_len < 1) {
		ESP_LOGE(TAG, "`/controls` URL too short, 400.");
		return send_400(p_request);
	}

	char* str_query = (char*) malloc(str_len);

	ifl(httpd_req_get_url_query_str(p_request, str_query, str_len) == ESP_FAIL) {
		ESP_LOGE(TAG, "`/controls` URL parsing failed!... 500.");
		free(str_query);
		return send_500(p_request);
	}

	char str_control[2];
	ifl(httpd_query_key_value(str_query, "control", str_control, sizeof(str_control)) == ESP_FAIL) {
		ESP_LOGE(TAG, "`/controls` parameter `control` not given. 400.");
		free(str_query);
		return send_400(p_request);
	}

	char str_event_data[5]; // `-`, three-digit number, `\0`. That's 5 `char`s.
	ifl(httpd_query_key_value(str_query, "value", str_event_data, sizeof(str_event_data)) == ESP_FAIL) {
		ESP_LOGE(TAG, "`/controls` parameter `value` not given. 400.");
		free(str_query);
		return send_400(p_request);
	}

	ESP_LOGE(TAG, "`/controls` URL OK! Checking parsed data...");
	ESP_LOGD(TAG, "`/controls` parameter `control` parsed string `%s`.", str_control);
	ESP_LOGD(TAG, "`/controls` parameter `event` parsed string `%s`.", str_event_data);
	free(str_query); // Can free this memory in advance, we won't need it now.
	// Now we have all the data we need! We can proceed with generating I2C messages.

	enum android_control_id const control = (enum android_control_id) atoi(str_control);

	switch (control) {

		case ANDROID_BUTTON_ID_STEER: {

			char *strtol_end;
			uint8_t value = strtol(str_event_data, &strtol_end, 10);

			ifu(
				strtol_end == str_event_data  // Did we get something *not* an control ID ordinal that couldn't be parsed?
				|| value < 0 || value > 255 // Is it not in `[0, 255]`?
				|| errno == ERANGE // Is it out of `long`-range?
			) {
				ESP_LOGE(TAG, "`/controls` parameter `control` not in range. 400.");
				return send_400(p_request);
			}

			i2c_message_arduino(MessageTypeEspCam::STEER);
			i2c_message_arduino(sizeof(value), &value);

			MessageTypeArduino message;

			i2c_await_arduino(sizeof(MessageTypeArduino));
			i2c_read_arduino(sizeof(MessageTypeArduino), (uint8_t*) &message);

			if (message != MessageTypeArduino::STEER_OK) {
				ESP_LOGE(TAG, "Arduino couldn't steer!... 500.");
				return send_500(p_request);
			}



		} break;

		case ANDROID_BUTTON_ID_FORWARD: {

		} break;

		case ANDROID_BUTTON_ID_BACKWARD: {

		} break;

	}

	ESP_LOGW(TAG, "`/controls` handler exited. Nothing to do!...");
	return ESP_OK;
}
