#include <freertos/FreeRTOS.h>
#include <esp_camera.h>

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>

// **Changed some files from the IDF to get this to build! (see `managed_components/espressif__arduino-esp32`):**
// - `AP.cpp`					(`managed_components/espressif__arduino-esp32/libraries/WiFi/src/AP.cpp`),
// - `STA.cpp`					(`managed_components/espressif__arduino-esp32/libraries/WiFi/src/STA.cpp`),
// - `WiFiGeneric.cpp`			(`managed_components/espressif__arduino-esp32/libraries/WiFi/src/WiFiGeneric.cpp`),
// - `chip-debug-report.cpp`	(`managed_components/espressif__arduino-esp32/cores/esp32/chip-debug-report.h`).
// Try diffing them with the "originals" you can obtain from the ESP Component Registry of the ESP-IDF extension.
// I also left some comments in some places, I think.
// I use IDF v5.1.1 - knowing this should help you know exactly what files to obtain from the ECR to diff against mine.

// *Original warning comment:*
// WARNING!!! PSRAM IC required for UXGA resolution and high JPEG quality
//            Ensure ESP32 Wrover Module or other board with PSRAM is selected
//            Partial images will be transmitted if image exceeds buffer size
//
//            You must select partition scheme from the board menu that has at least 3MB APP space.
//            Face Recognition is DISABLED for ESP32 and ESP32-S2, because it takes up from 15
//            seconds to process single frame. Face Detection is ENABLED if PSRAM is enabled as well

#define CAMERA_MODEL_AI_THINKER // We've got *some* PSRAM! I don't remember how much exactly. Sorry.
#include "camera_pins.h"

const char *ssid = "Tech Creator";
const char *password = "ThisIsNotSecure";
// Do these really get "leaked"? They aren't encryption *keys!*

extern void startCameraServer();
// extern void setupLedFlash(int pin);

extern "C" void app_main() {
	initArduino();

	Serial.begin(115200);
	Serial.setDebugOutput(true);
	Serial.println();

	camera_config_t config;

	config.fb_count = 1;
	config.jpeg_quality = 12;

	config.pin_d0 = Y2_GPIO_NUM;
	config.pin_d1 = Y3_GPIO_NUM;
	config.pin_d2 = Y4_GPIO_NUM;
	config.pin_d3 = Y5_GPIO_NUM;
	config.pin_d4 = Y6_GPIO_NUM;
	config.pin_d5 = Y7_GPIO_NUM;
	config.pin_d6 = Y8_GPIO_NUM;
	config.pin_d7 = Y9_GPIO_NUM;

	config.pin_pclk = PCLK_GPIO_NUM;
	config.pin_xclk = XCLK_GPIO_NUM;

	config.pin_href = HREF_GPIO_NUM;
	config.pin_pwdn = PWDN_GPIO_NUM;
	config.pin_reset = RESET_GPIO_NUM;
	config.pin_vsync = VSYNC_GPIO_NUM;

	config.pin_sccb_scl = SIOC_GPIO_NUM;
	config.pin_sccb_sda = SIOD_GPIO_NUM;

	config.ledc_timer = LEDC_TIMER_0;
	config.ledc_channel = LEDC_CHANNEL_0;

	config.xclk_freq_hz = 20000000;
	config.frame_size = FRAMESIZE_UXGA;
	config.pixel_format = PIXFORMAT_JPEG; // JPEG for streaming. Use `PIXFORMAT_RGB565` for best face detection and recognition.

	config.fb_location = CAMERA_FB_IN_PSRAM;
	config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

	// If we have PSRAM, go with a UXGA resolution instead. Also, higher JPEG quality!
	// Why? Because now we *can* have a *larger* pre-allocated frame buffer.
	if (config.pixel_format == PIXFORMAT_JPEG) {

		if (psramFound()) {
			config.fb_count = 2;
			config.jpeg_quality = 10;
			config.grab_mode = CAMERA_GRAB_LATEST;
		} else {
			// No PSRAM :(
			config.frame_size = FRAMESIZE_SVGA;
			config.fb_location = CAMERA_FB_IN_DRAM;
		}

	} else {
		// Best option for face detection and recognition:
		config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3 // This chip has enough DRAM for two frames!
		config.fb_count = 2;
#endif
	}

#if defined(CAMERA_MODEL_ESP_EYE) // Pins `13` and `14` are `INPUT_PULLUP` pins...
	pinMode(13, INPUT_PULLUP);
	pinMode(14, INPUT_PULLUP);
#endif

	// This variable is not reused after this *one* check:
	esp_err_t const err = esp_camera_init(&config);
	if (err != ESP_OK) {
		Serial.printf("Camera init failed with error 0x%x", err);
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
	if (config.pixel_format == PIXFORMAT_JPEG) {
		sensor->set_framesize(sensor, FRAMESIZE_QVGA);
	}

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
	sensor->set_vflip(sensor, 1);
	sensor->set_hmirror(sensor, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
	sensor->set_vflip(sensor, 1);
#endif

	// Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
	// setupLedFlash(LED_GPIO_NUM);
#endif

	WiFi.begin(ssid, password);
	WiFi.setSleep(false);

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}

	Serial.println("");
	Serial.println("WiFi connected!");

	Wire.begin();
	Serial.println("I2C begun.");

	startCameraServer();

	Serial.print("Camera stream is now available on `http://");
	Serial.print(WiFi.localIP());
	Serial.println(":81/stream`. Enjoy!");
}
