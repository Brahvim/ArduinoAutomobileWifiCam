#include <esp_chip_info.h>
#include <esp_heap_caps.h>
#include <esp_camera.h>
#include <esp_system.h>
#include <esp_psram.h>
#include <esp_wifi.h>
#include <esp_log.h>

#include <freertos/event_groups.h>
#include <freertos/FreeRTOS.h>
#include <driver/ledc.h>
#include <nvs_flash.h>

#include "protocol_car_controls.hpp"
#include "app.h"

// **Changed some files from the IDF to get this to build! (see `managed_components/espressif__arduino-esp32`):**
// - `AP.cpp`					(`managed_components/espressif__arduino-esp32/libraries/WiFi/src/AP.cpp`),
// - `STA.cpp`					(`managed_components/espressif__arduino-esp32/libraries/WiFi/src/STA.cpp`),
// - `WiFiGeneric.cpp`			(`managed_components/espressif__arduino-esp32/libraries/WiFi/src/WiFiGeneric.cpp`),
// - `chip-debug-report.cpp`	(`managed_components/espressif__arduino-esp32/cores/esp32/chip-debug-report.h`).
// Try diffing them with the "originals" you can obtain from the ESP Component Registry of the ESP-IDF extension.
// I also left some comments in some places, I think.
// I use IDF v5.1.1 - knowing this should help you know exactly what files to obtain from the ECR to diff against mine.

#define CAMERA_MODEL_AI_THINKER // We've got *some* PSRAM! I don't remember how much exactly. Sorry.
#include "camera_pins.h"
#include "app_controls.hpp"
#include "protocol_car_controls.hpp"

const char *TAG = __FILE__;
const char *ssid = "Tech Creator"; // Don't ask me why it's called this; ask **management**. ***MANAGEMENT!***
const char *password = "ThisIsNotSecure"; // It indeed isn't, because this is available online. *Though...*
// ...Do these really get "leaked"? They aren't encryption *keys!* Come on, you won't suddenly be near my phone next year!

#define WIFI_CONNECTED_BIT BIT0

EventGroupHandle_t g_wifi_event_group;

extern void startCameraServer();
// extern void setupLedFlash(int pin);

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

				startCameraServer();

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

			.ssid = "Tech Creator",
			.password = "ThisIsNotSecure",

		},

		// .nan = NULL,

	};
#pragma GCC diagnostic pop

	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config_wifi));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_LOGI(TAG, "WiFi started.");
}

extern "C" void app_main() {

	// esp_chip_info_t chip_info;
	// esp_chip_info(&chip_info);

	// ESP_LOGI("Chip Info", "Cores: %d", chip_info.cores);
	// ESP_LOGI("Chip Info", "Chip model: %d", chip_info.model);
	// ESP_LOGI("Chip Info", "Revision: %d", chip_info.revision);
	// ESP_LOGI("Chip Info", "WiFi capabilities: %lu", chip_info.features);

	// ESP_ERROR_CHECK(esp_psram_init()); // Results in an `ESP_ERR_INVALID_STATE`, because... PSRAM was init'ed *already?!*
	ESP_ERROR_CHECK(nvs_flash_init());

	ESP_LOGI(TAG, "PSRAM size? `%zu` bytes!", esp_psram_get_size());
	// ESP_LOGI(TAG, "PSRAM? %s", esp_psram_is_initialized() ? "Yep!" : "Nope...");

	camera_config_t config_camera;

	config_camera.pin_d0 = Y2_GPIO_NUM;
	config_camera.pin_d1 = Y3_GPIO_NUM;
	config_camera.pin_d2 = Y4_GPIO_NUM;
	config_camera.pin_d3 = Y5_GPIO_NUM;
	config_camera.pin_d4 = Y6_GPIO_NUM;
	config_camera.pin_d5 = Y7_GPIO_NUM;
	config_camera.pin_d6 = Y8_GPIO_NUM;
	config_camera.pin_d7 = Y9_GPIO_NUM;

	config_camera.pin_pclk = PCLK_GPIO_NUM;
	config_camera.pin_xclk = XCLK_GPIO_NUM;

	config_camera.pin_href = HREF_GPIO_NUM;
	config_camera.pin_pwdn = PWDN_GPIO_NUM;
	config_camera.pin_reset = RESET_GPIO_NUM;
	config_camera.pin_vsync = VSYNC_GPIO_NUM;

	config_camera.pin_sccb_scl = SIOC_GPIO_NUM;
	config_camera.pin_sccb_sda = SIOD_GPIO_NUM;

	config_camera.xclk_freq_hz = 20000000;
	config_camera.ledc_timer = LEDC_TIMER_0;
	config_camera.ledc_channel = LEDC_CHANNEL_0;

	config_camera.fb_count = 2;
	config_camera.jpeg_quality = 12;
	config_camera.frame_size = FRAMESIZE_SVGA;
	config_camera.pixel_format = PIXFORMAT_JPEG; // JPEG for streaming. Use `PIXFORMAT_RGB565` for best face detection and recognition.
	config_camera.fb_location = CAMERA_FB_IN_PSRAM;
	config_camera.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

	// This variable is not reused after this *one* check:
	esp_err_t const err = esp_camera_init(&config_camera);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Camera init failed with error %s", esp_err_to_name(err));
		return;
	}

	sensor_t *sensor = esp_camera_sensor_get(); // Can't be a `const*`, see method calls in conditional compilation blocks below.
	// Initially, the sensors are flipped vertically. The colors are a *bit* saturated. We aim to correct that here:
	if (sensor->id.PID == OV3660_PID) {
		sensor->set_vflip(sensor, 1);		// ...Flip it back,
		sensor->set_brightness(sensor, 1);  // Up the brightness *just a bit*.
		sensor->set_saturation(sensor, -2); // Lower the saturation.
	}

	// Drop down the frame-size for a higher *initial frame-rate:*
	if (config_camera.pixel_format == PIXFORMAT_JPEG) {
		sensor->set_framesize(sensor, FRAMESIZE_QVGA);
	}

	// Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
	// setupLedFlash(LED_GPIO_NUM);
#endif

	// Modding these into `INPUT` pins might help the Arduino not pick up on these:
	// pinMode(CAR_PIN_DIGITAL_ESP_CAM_STEER, OUTPUT);
	// pinMode(CAR_PIN_DIGITAL_ESP_CAM_1, OUTPUT);
	// pinMode(CAR_PIN_DIGITAL_ESP_CAM_2, OUTPUT);

	wifi_init_sta();
	// ESP_LOGI(TAG, "Left RAM: `%lu` bytes.", esp_get_free_heap_size());
}
