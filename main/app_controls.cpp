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

// I2C stuff.
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

// `i2c_message_arduino()` overloads.
esp_err_t i2c_message_arduino(size_t const p_bytes, void* p_data) {
	Wire.beginTransmission(I2C_ADDR);
	size_t const bytes_sent = Wire.write((uint8_t*) p_data, p_bytes);
	uint8_t const wire_status = Wire.endTransmission(false);

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
	// Wire.beginTransmission(I2C_ADDR);
	// size_t const bytes_sent = Wire.write(resultMessageTypeData, sizeof(MessageTypeEspCam));
	// uint8_t const wire_status = Wire.endTransmission(false);

	ESP_LOGI(TAG, "Arduino `Wire`/I2C transmission **to send** `%zu` bytes.", sizeof(MessageTypeEspCam));

	Wire.beginTransmission(I2C_ADDR);
	size_t const bytes_sent = Wire.write((uint8_t*) &p_command, sizeof(MessageTypeEspCam));
	uint8_t const wire_status = Wire.endTransmission(false);

	ifu(bytes_sent != sizeof(MessageTypeEspCam)) {
		ESP_LOGE(TAG, "Arduino `Wire`/I2C transmission didn't send all bytes for message transaction.");
		return ESP_FAIL;
	}

	ESP_LOGI(TAG, "Arduino `Wire`/I2C transmission wrote `%zu` bytes.", bytes_sent);

	ifu(wire_status) {
		ESP_LOGE(TAG, "Arduino `Wire`/I2C message transmission failure: \"%s\".", wire_status_to_string(wire_status));
		return ESP_FAIL;
	}

	return ESP_OK;
}

esp_err_t i2c_message_arduino(size_t const p_bytes, void* p_data, MessageTypeEspCam const p_command) {
	Wire.beginTransmission(I2C_ADDR);

	size_t const bytes_sent_command = Wire.write((uint8_t*) p_data, p_bytes);
	ifu(bytes_sent_command != p_bytes) {
		ESP_LOGE(TAG, "Arduino `Wire`/I2C transmission didn't send all bytes for data transaction.");
		return ESP_FAIL;
	}

	size_t const bytes_sent_data = Wire.write((uint8_t*) p_data, p_bytes);
	uint8_t const wire_status = Wire.endTransmission(false);

	ifu(bytes_sent_data != p_bytes) {
		ESP_LOGE(TAG, "Arduino `Wire`/I2C transmission didn't send all bytes for data transaction.");
		return ESP_FAIL;
	}

	ifu(!wire_status) {
		ESP_LOGE(TAG, "Arduino `Wire`/I2C data transmission status: \"%s\".", wire_status_to_string(wire_status));
		return ESP_FAIL;
	}

	return ESP_OK;
}

MessageTypeArduino i2c_parse_message_arduino() {
	MessageTypeArduino to_ret;
	i2c_read_arduino(sizeof(MessageTypeArduino), (uint8_t*) &to_ret);
	return to_ret;
}

esp_err_t i2c_read_arduino(size_t const p_bytes, uint8_t *p_buffer) {
	for (int i = 0; i < p_bytes; ++i) {
		p_buffer[i] = Wire.read(); // Compiler won't unroll this :(
	}

	return ESP_OK;
}

MessageTypeArduino i2c_await_message_arduino(size_t const p_interval_millis, size_t const p_interval_count) {
	i2c_await_arduino(sizeof(MessageTypeArduino), p_interval_millis, p_interval_count);
	return i2c_parse_message_arduino();
}

esp_err_t i2c_await_arduino(int const p_bytes, size_t const p_interval_millis, size_t const p_interval_count) {
	Wire.requestFrom(I2C_ADDR, sizeof(MessageTypeArduino));

	for (size_t i = p_interval_count; i < 1 || Wire.available() > p_bytes; --i) {
		delay(p_interval_millis);
	}

	return ESP_OK;
}

// HTTP stuff.
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

esp_err_t android_controls_handler_i2c(httpd_req_t *p_request) {
	size_t const str_query_len = 1 + httpd_req_get_url_query_len(p_request);
	httpd_resp_set_type(p_request, "application/octet-stream");

	ESP_LOGD(TAG, "`/controls` queried!");
	ESP_LOGD(TAG, "`/controls` query length `%zu`!", str_query_len);

	ifu(str_query_len < 2) {
		ESP_LOGE(TAG, "`/controls` URL too short, 400.");
		send_400(p_request);
		return ESP_OK;
	}

	char* str_query = (char*) malloc(str_query_len);

	if (str_query == NULL) {
		ESP_LOGE(TAG, "`/controls` URL parsing failed due to `NULL` return from `malloc()`. 500.");
		return send_500(p_request);
	}

	esp_err_t err;
	ifl((err = httpd_req_get_url_query_str(p_request, str_query, str_query_len)) != ESP_OK) {
		ESP_LOGE(TAG, "`/controls` URL parsing failed! Reason: \"%s\". 500.", esp_err_to_name(err));
		free(str_query);
		return send_500(p_request);
	}

	ESP_LOGI(TAG, "`/controls` received HTTP query `%s`.", str_query);

	char str_control[20]; // 2
	ifl((err = httpd_query_key_value(str_query, "control", str_control, sizeof(str_control))) != ESP_OK) {
		ESP_LOGE(TAG, "`/controls` parameter `control` not parsed. 400.");
		free(str_query);
		send_400(p_request);
		return ESP_OK;
	}

	char str_event_data[20]; // 5, `-`, three-digit number, `\0`. That's 5 `char`s.
	ifl((err = httpd_query_key_value(str_query, "value", str_event_data, sizeof(str_event_data))) != ESP_OK) {
		ESP_LOGE(TAG, "`/controls` parameter `value` not parsed. Reason: \"%s\". 400.", esp_err_to_name(err));
		free(str_query);
		send_400(p_request);
		return ESP_OK;
	}

	ESP_LOGD(TAG, "`/controls` URL OK! Checking parsed data...");
	ESP_LOGI(TAG, "`/controls` parameter `control` parsed string `%s`.", str_control);
	ESP_LOGI(TAG, "`/controls` parameter `event` parsed string `%s`.", str_event_data);

	// Can free this memory in advance, we won't need it now.
	// Now we have all the data we need! We can proceed with generating I2C messages.
	free(str_query);

	char *strtol_end_control;
	enum android_control_id const control = (enum android_control_id) strtol(str_event_data, &strtol_end_control, 10);

	ifu(
		strtol_end_control == str_event_data  // Did we get something *not* an control ID ordinal that couldn't be parsed?
		|| control < ANDROID_BUTTON_ID_STEER || control > ANDROID_BUTTON_ID_BACKWARD // Is it not in `[0, 255]`?
		|| errno == ERANGE // Is it out of `long`-range?
	) {
		ESP_LOGE(TAG, "`/controls` parameter `control` could not be parsed as a number. 400.");
		send_400(p_request);
		return ESP_OK;
	}

	switch (control) { // Actually go execute stuff.

		case ANDROID_BUTTON_ID_STEER: {
			char *strtol_end;
			uint8_t value = strtol(str_event_data, &strtol_end, 10);

			errno = 0;
			ifu(
				strtol_end == str_event_data  // Did we get something *not* an control ID ordinal that couldn't be parsed?
				|| value < 0 || value > 255 // Is it not in `[0, 255]`?
				|| errno == ERANGE // Is it out of `long`-range?
			) {
				ESP_LOGE(TAG, "`/controls` parameter `control` not in range for `ANDROID_BUTTON_ID_STEER`. 400.");
				send_400(p_request);
				return ESP_OK;
			}

			i2c_message_arduino(sizeof(value), &value, MessageTypeEspCam::STEER);

			ifu(i2c_await_message_arduino() == MessageTypeArduino::STEER_OK) {
				send_200(p_request);
				return ESP_OK;
			} else {
				ESP_LOGE(TAG, "Arduino couldn't steer!... 500.");
				send_500(p_request);
			}
		} break;

		case ANDROID_BUTTON_ID_FORWARD: {
			char *strtol_end;
			enum android_button_event const value = (enum android_button_event) strtol(str_event_data, &strtol_end, 10);

			errno = 0;
			ifu(
				strtol_end == str_event_data  // Did we get something *not* an control ID ordinal that couldn't be parsed?
				|| value < ANDROID_BUTTON_EVENT_PRESSED || value > ANDROID_BUTTON_EVENT_RELEASED // Is it not in the `enum`'s domain?
				|| errno == ERANGE // Is it out of `long`-range?
			) {
				ESP_LOGE(TAG, "`/controls` parameter `control` not in range for `ANDROID_BUTTON_ID_FORWARD`. 400.");
				send_400(p_request);
				return ESP_OK;
			}

			switch (value) {

				case ANDROID_BUTTON_EVENT_PRESSED: {
					i2c_message_arduino(MessageTypeEspCam::GEAR_FORWARD);

					ifu(i2c_await_message_arduino() == MessageTypeArduino::GEAR_OK) {
						send_200(p_request);
						return ESP_OK;
					} else {
						ESP_LOGE(TAG, "Arduino couldn't gear!... 500.");
						send_500(p_request);
					}
				} break;

				case ANDROID_BUTTON_EVENT_RELEASED: {
					i2c_message_arduino(MessageTypeEspCam::STOP);

					ifu(i2c_await_message_arduino() == MessageTypeArduino::STOP_OK) {
						send_200(p_request);
						return ESP_OK;
					} else {
						ESP_LOGE(TAG, "Arduino couldn't stop!... 500.");
						send_500(p_request);
					}
				} break;

			}

		} break;

		case ANDROID_BUTTON_ID_BACKWARD: {
			char *strtol_end;
			enum android_button_event const value = (enum android_button_event) strtol(str_event_data, &strtol_end, 10);

			errno = 0;
			ifu(
				strtol_end == str_event_data  // Did we get something *not* an control ID ordinal that couldn't be parsed?
				|| value < ANDROID_BUTTON_EVENT_PRESSED || value > ANDROID_BUTTON_EVENT_RELEASED // Is it not in the `enum`'s domain?
				|| errno == ERANGE // Is it out of `long`-range?
			) {
				ESP_LOGE(TAG, "`/controls` parameter `control` not in range for `ANDROID_BUTTON_ID_BACKWARD`. 400.");
				send_400(p_request);
				return ESP_OK;
			}

			switch (value) {

				case ANDROID_BUTTON_EVENT_PRESSED: {
					i2c_message_arduino(MessageTypeEspCam::GEAR_BACKWARD);

					ifu(i2c_await_message_arduino() == MessageTypeArduino::GEAR_OK) {
						send_200(p_request);
						return ESP_OK;
					} else {
						ESP_LOGE(TAG, "Arduino couldn't gear!... 500.");
						send_500(p_request);
					}
				} break;

				case ANDROID_BUTTON_EVENT_RELEASED: {
					i2c_message_arduino(MessageTypeEspCam::STOP);

					ifu(i2c_await_message_arduino() == MessageTypeArduino::STOP_OK) {
						send_200(p_request);
						return ESP_OK;
					} else {
						ESP_LOGE(TAG, "Arduino couldn't stop!... 500.");
						send_500(p_request);
					}
				} break;

			}

		} break;

	}

	ESP_LOGW(TAG, "`/controls` handler exited. Nothing to do!...");
	return ESP_OK;
}

esp_err_t android_controls_handler(httpd_req_t *p_request) {
	size_t const str_query_len = 1 + httpd_req_get_url_query_len(p_request);
	httpd_resp_set_type(p_request, "application/octet-stream");

	ESP_LOGD(TAG, "`/controls` queried!");
	ESP_LOGD(TAG, "`/controls` query length `%zu`!", str_query_len);

	ifu(str_query_len < 2) {
		ESP_LOGE(TAG, "`/controls` URL too short, 400.");
		send_400(p_request);
		return ESP_OK;
	}

	char *str_query = (char*) calloc(str_query_len, sizeof(char));

	if (str_query == NULL) {
		ESP_LOGE(TAG, "`/controls` URL parsing failed due to `NULL` return from `malloc()`. 500.");
		send_500(p_request);
		// No free 👍️
		return ESP_OK;
	}

	esp_err_t err;
	ifl((err = httpd_req_get_url_query_str(p_request, str_query, str_query_len)) != ESP_OK) {
		ESP_LOGE(TAG, "`/controls` URL parsing failed! Reason: \"%s\". 500.", esp_err_to_name(err));
		send_500(p_request);
		free(str_query);
		return ESP_OK;
	}

	ESP_LOGI(TAG, "`/controls` query `%s`.", str_query);

	char str_control[20]; // Actually needs only `2` - digit and `\0`.
	ifl((err = httpd_query_key_value(str_query, "control", str_control, sizeof(str_control))) != ESP_OK) {
		ESP_LOGE(TAG, "`/controls` parameter `control` not parsed. 400.");
		send_400(p_request);
		free(str_query);
		return ESP_OK;
	}

	char str_event_data[20]; // Needs only `5`, `-`, three-digit number, `\0`. That's 5 `char`s.
	ifl((err = httpd_query_key_value(str_query, "value", str_event_data, sizeof(str_event_data))) != ESP_OK) {
		ESP_LOGE(TAG, "`/controls` parameter `value` not parsed. Reason: \"%s\". 400.", esp_err_to_name(err));
		send_400(p_request);
		free(str_query);
		return ESP_OK;
	}

	ESP_LOGD(TAG, "`/controls` URL OK! Checking parsed data...");
	ESP_LOGI(TAG, "`/controls` parameter `control` parsed string `%s`.", str_control);
	ESP_LOGI(TAG, "`/controls` parameter `event` parsed string `%s`.", str_event_data);

	// Can free this memory in advance, we won't need it now.
	// Now we have all the data we need! We can proceed with generating I2C messages.
	free(str_query);

	char *strtol_end_control;
	enum android_control_id const control = (enum android_control_id) strtol(str_event_data, &strtol_end_control, 10);

	ifu( // false
		strtol_end_control == str_event_data  // Did we get something *not* an control ID ordinal that couldn't be parsed?
		|| control < ANDROID_BUTTON_ID_STEER || control > ANDROID_BUTTON_ID_BACKWARD // Is it not in `[0, 255]`?
		|| errno == ERANGE // Is it out of `long`-range?
	) {
		ESP_LOGE(TAG, "`/controls` parameter `control` could not be parsed as a number. 400.");
		send_400(p_request);
		return ESP_OK;
	}

	pinMode(14, OUTPUT);
	pinMode(15, OUTPUT);

	switch (control) { // Actually go execute stuff.

		case ANDROID_BUTTON_ID_STEER: {
			char *strtol_end;
			uint8_t const value = strtol(str_event_data, &strtol_end, 10);

			errno = 0;
			ifu(
				strtol_end == str_event_data  // Did we get something *not* an control ID ordinal that couldn't be parsed?
				|| value > 255 // Is it not in `[0, 255]`? (It's also `unsigned`, so `value < 0` is impossible.)
				|| errno == ERANGE // Is it out of `long`-range?
			) {
				ESP_LOGE(TAG, "`/controls` parameter `control` not in range for `ANDROID_BUTTON_ID_STEER`. 400.");
				send_400(p_request);
				return ESP_OK;
			}

			analogWrite(14, value);
			ESP_LOGI(TAG, "Wrote steer value `%d`.", value);

			// pinMode(14, INPUT);
			// int const reading = analogRead(14);

			// ifu(reading == (uint8_t) MessageTypeArduino::STEER_OK) {
			send_200(p_request);
			return ESP_OK;
			// } else {
			// 	ESP_LOGE(TAG, "Arduino couldn't steer!... 500.");
			// 	send_500(p_request);
			// }
		} break;

		case ANDROID_BUTTON_ID_FORWARD: {
			char *strtol_end;
			enum android_button_event const value = (enum android_button_event) strtol(str_event_data, &strtol_end, 10);

			errno = 0;
			ifu(
				strtol_end == str_event_data  // Did we get something *not* an control ID ordinal that couldn't be parsed?
				|| value < ANDROID_BUTTON_EVENT_PRESSED || value > ANDROID_BUTTON_EVENT_RELEASED // Is it not in the `enum`'s domain?
				|| errno == ERANGE // Is it out of `long`-range?
			) {
				ESP_LOGE(TAG, "`/controls` parameter `control` not in range for `ANDROID_BUTTON_ID_FORWARD`. 400.");
				send_400(p_request);
				return ESP_OK;
			}

			switch (value) {

				case ANDROID_BUTTON_EVENT_PRESSED: {
					// i2c_message_arduino(MessageTypeEspCam::GEAR_FORWARD);
					analogWrite(15, 1024);
					ESP_LOGI(TAG, "Car should go forward!");

					// pinMode(15, INPUT);
					// ifu(analogRead(15) == (uint8_t) MessageTypeArduino::GEAR_OK) {
					send_200(p_request);
					return ESP_OK;
					// } else {
					// 	ESP_LOGE(TAG, "Arduino couldn't gear!... 500.");
					// 	send_500(p_request);
					// }
				} break;

				case ANDROID_BUTTON_EVENT_RELEASED: {
					analogWrite(15, 512);
					ESP_LOGI(TAG, "Car should remain neutral...");

					// 	pinMode(15, INPUT);
					// 	ifu(analogRead(15) == (uint8_t) MessageTypeArduino::STOP_OK) {
					send_200(p_request);
					return ESP_OK;
					// 	} else {
					// 		ESP_LOGE(TAG, "Arduino couldn't stop!... 500.");
					// 		send_500(p_request);
					// 	}
				} break;

			}

			ESP_LOGE(TAG, "Nothing to send!!! `400`!!!");
			send_400(p_request);
			return ESP_OK;
		} break;

		case ANDROID_BUTTON_ID_BACKWARD: {
			char *strtol_end;
			enum android_button_event const value = (enum android_button_event) strtol(str_event_data, &strtol_end, 10);

			errno = 0;
			ifu(
				strtol_end == str_event_data  // Did we get something *not* an control ID ordinal that couldn't be parsed?
				|| value < ANDROID_BUTTON_EVENT_PRESSED || value > ANDROID_BUTTON_EVENT_RELEASED // Is it not in the `enum`'s domain?
				|| errno == ERANGE // Is it out of `long`-range?
			) {
				ESP_LOGE(TAG, "`/controls` parameter `control` not in range for `ANDROID_BUTTON_ID_BACKWARD`. 400.");
				send_400(p_request);
				return ESP_OK;
			}

			switch (value) {

				case ANDROID_BUTTON_EVENT_PRESSED: {
					// i2c_message_arduino(MessageTypeEspCam::GEAR_BACKWARD);
					analogWrite(15, 0);
					ESP_LOGI(TAG, "Car should go backwards!");

					// pinMode(15, INPUT);
					// ifu(analogRead(15) == (uint8_t) MessageTypeArduino::GEAR_OK) {
					send_200(p_request);
					return ESP_OK;
					// } else {
					// 	ESP_LOGE(TAG, "Arduino couldn't gear!... 500.");
					// 	send_500(p_request);
					// }
				} break;

				case ANDROID_BUTTON_EVENT_RELEASED: {
					// i2c_message_arduino(MessageTypeEspCam::STOP);
					analogWrite(15, 512);
					ESP_LOGI(TAG, "Car should remain neutral...");

					// pinMode(15, INPUT);
					// ifu(analogRead(15) == (uint8_t) MessageTypeArduino::STOP_OK) {
					send_200(p_request);
					return ESP_OK;
					// } else {
					// 	ESP_LOGE(TAG, "Arduino couldn't stop!... 500.");
					// 	send_500(p_request);
					// }
				} break;

			}

			ESP_LOGE(TAG, "Nothing to send!!! `400`!!!");
			send_400(p_request);
			return ESP_OK;
		} break;

	}

	ESP_LOGW(TAG, "`/controls` handler exited. Nothing to do!...");
	return ESP_OK;
}
