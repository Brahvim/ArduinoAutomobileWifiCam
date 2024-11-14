#pragma once

#include <esp_system.h>
#include <esp_event_base.h>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#define ifu(x) if (__builtin_expect((x), 0))
#define ifl(x) if (__builtin_expect((x), 1))

extern EventGroupHandle_t g_wifi_event_group;

void wifi_init_sta();
void start_camera_server();
void event_handler_wifi(void *param, esp_event_base_t event_base, int32_t event_id, void *event_data);
