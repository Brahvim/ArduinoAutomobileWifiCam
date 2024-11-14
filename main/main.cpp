#include <esp_camera.h>
#include <esp_wifi.h>
#include <esp_log.h>

#include <nvs_flash.h>

#include "protocol_car_controls.hpp"
#include "app_http_endpoints.h"
#include "app.h"

#define WIFI_CONNECTED_BIT BIT0

EventGroupHandle_t g_wifi_event_group;

static const char *TAG = __FILE__;

void event_handler_wifi(void *p_param, esp_event_base_t p_event_base, int32_t p_event_id, void *p_event_data) {
	if (p_event_base == WIFI_EVENT) {

		switch (p_event_id) {

			case WIFI_EVENT_STA_DISCONNECTED: {

				ESP_LOGI(TAG, "Wi-Fi disconnection event occurred...");
				esp_wifi_connect();

			} break;

			case WIFI_EVENT_STA_CONNECTED: {

				ESP_LOGI(TAG, "Wi-Fi connection event occurred...");
				esp_wifi_connect();

			} break;

			case WIFI_EVENT_STA_START: {

				ESP_LOGI(TAG, "Wi-Fi start event occurred...");
				esp_wifi_connect();

			} break;

		}

	} else if (p_event_base == IP_EVENT) {

		switch (p_event_id) {

			case IP_EVENT_ETH_GOT_IP: {

				ESP_LOGI(TAG, "IP ethernet \"Got IP\" event occurred...");
				esp_wifi_connect();

			} break;

			case IP_EVENT_STA_GOT_IP: {

				// ip_event_got_ip_t const *event_got_ip = (ip_event_got_ip_t*) p_event_data;
				// auto ip = event_got_ip->ip_info.ip;
				// ESP_LOGI(TAG, "ESP32-CAM IP address is now `" IPSTR "`.", IP2STR(ip));

				for (size_t i = 0; i < 25; ++i) {
					ESP_LOGI(TAG, "Connected!");
				}

				start_camera_server();

				// Friendly URL logs!

				// ESP_LOGI(TAG, "Camera stream! ...Now available on `http://%s:81/stream`. Enjoy!\n", ipStr);
				// ESP_LOGI(TAG, "Controls also available!:");

				// ESP_LOGI(TAG, "- Visit / `curl` to move the car backwards:");
				// ESP_LOGI(TAG, "  `http://%s/controls?gear=B`.\n", ipStr);

				// ESP_LOGI(TAG, "- Visit / `curl` to move the car forwards:");
				// ESP_LOGI(TAG, "  `http://%s/controls?gear=F`.\n", ipStr);

				// ESP_LOGI(TAG, "- Visit / `curl` to stop the car entirely:");
				// ESP_LOGI(TAG, "  `http://%s/controls?gear=N`.\n", ipStr);

				// ESP_LOGI(TAG, "- Visit / `curl` to steer the car straight:");
				// ESP_LOGI(TAG, "  `http://%s/controls?steer=127`.\n", ipStr);

				// ESP_LOGI(TAG, "- Visit / `curl` to steer the car right:");
				// ESP_LOGI(TAG, "  `http://%s/controls?steer=255`.\n", ipStr);

				// ESP_LOGI(TAG, "- Visit / `curl` to steer the car left:");
				// ESP_LOGI(TAG, "  `http://%s/controls?steer=0`.\n", ipStr);

			} break;

			default: {

				ESP_LOGI(TAG, "IP event `%ld` occurred!", p_event_id);

			} break;

		}

	}
}

extern "C" void app_main() {
	ESP_ERROR_CHECK(nvs_flash_init());

	camera_config_t config_camera;

	config_camera.pin_d0 = 5;
	config_camera.pin_d1 = 18;
	config_camera.pin_d2 = 19;
	config_camera.pin_d3 = 21;
	config_camera.pin_d4 = 36;
	config_camera.pin_d5 = 39;
	config_camera.pin_d6 = 34;
	config_camera.pin_d7 = 35;

	config_camera.pin_xclk = 0;
	config_camera.pin_pclk = 22;

	config_camera.pin_href = 23;
	config_camera.pin_pwdn = 32;
	config_camera.pin_reset = -1;
	config_camera.pin_vsync = 25;

	config_camera.pin_sccb_scl = 27;
	config_camera.pin_sccb_sda = 26;

	config_camera.xclk_freq_hz = 20000000;
	config_camera.ledc_timer = LEDC_TIMER_0;
	config_camera.ledc_channel = LEDC_CHANNEL_0;

	config_camera.fb_count = 1;
	config_camera.jpeg_quality = 63;
	config_camera.frame_size = FRAMESIZE_VGA;
	config_camera.pixel_format = PIXFORMAT_JPEG; // `PIXFORMAT_JPEG` for streaming. `PIXFORMAT_RGB565` for face detection and recognition!
	config_camera.fb_location = CAMERA_FB_IN_PSRAM;
	config_camera.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

	ESP_ERROR_CHECK(esp_camera_init(&config_camera));

	// Drop down the frame-size for a higher *initial frame-rate*. Use only with `PIXFORMAT_JPEG`...?:
	// if (config_camera.pixel_format == PIXFORMAT_JPEG) {
	sensor_t *sensor = esp_camera_sensor_get(); // Can't be a `const*`, see method calls in conditional compilation blocks below.
	sensor->set_framesize(sensor, FRAMESIZE_VGA);
	// }

	// Modding these into `INPUT` pins might help the Arduino not pick up on these:
	// pinMode(CAR_PIN_DIGITAL_ESP_CAM_STEER, OUTPUT);
	// pinMode(CAR_PIN_DIGITAL_ESP_CAM_1, OUTPUT);
	// pinMode(CAR_PIN_DIGITAL_ESP_CAM_2, OUTPUT);

	wifi_init_sta();
}

void wifi_init_sta() {
	g_wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	ESP_ERROR_CHECK(esp_netif_init());

	esp_netif_create_default_wifi_sta();

	wifi_init_config_t config_wifi_init = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&config_wifi_init));

	ESP_ERROR_CHECK(esp_event_handler_instance_register(
		WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler_wifi, NULL, NULL
	));

	ESP_ERROR_CHECK(esp_event_handler_instance_register(
		// IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler_wifi, NULL, NULL
		IP_EVENT, ESP_EVENT_ANY_ID, &event_handler_wifi, NULL, NULL
	));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
	wifi_config_t config_wifi = {

		// .ap = NULL,

		.sta = {

			.ssid = "Tech Creator", // Don't ask me why it's called this; ask **management**. ***MANAGEMENT!***
			.password = "ThisIsNotSecure", // It indeed isn't, because this is available online. *Though...*
			// ...Do these really get "leaked"? They aren't encryption *keys!* Come on, you won't suddenly be near my phone next year!

		},

		// .nan = NULL,

	};
#pragma GCC diagnostic pop

	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config_wifi));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_LOGI(TAG, "WiFi started.");
}
