#include <Arduino.h>
#include <freertos/FreeRTOS.h>

extern void loop();
extern void setup();

void loopTask(void *p_parameters) {
	setup();

	// Arduino-style loop:
	while (true) {
		loop();
		vTaskDelay(1); // Don't be a CPU hog!
	}
}

extern "C" void app_main() {
	initArduino();

	xTaskCreateUniversal(
		loopTask,   // Task function,
		"loop", 	// Task name,
		8192,       // Stack size,
		NULL,       // Parameters to pass,
		1,          // Priority,
		NULL,       // Task handle,
		1           // Run on core 1 of cores {0, 1}. Setting this forces using `xTaskCreatePinnedToCore()` instead of `xTaskCreate()`.
	);
}
