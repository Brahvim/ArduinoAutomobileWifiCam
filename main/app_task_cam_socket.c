#include <freertos/FreeRTOS.h>
#include <lwip/sockets.h>

#include "app_task_cam_socket.h"

#define to_string(x) #x

static TaskHandle_t s_task_cam_socket;

struct task_cam_socket_params {

	char const *client_ip;

};

void task_cam_socket_shutdown() {
}

void task_cam_socket_start() {
	xTaskCreate(task_cam_socket, to_string(task_cam_socket), 2048, NULL, 2, &s_task_cam_socket);
}

void task_cam_socket(void *p_param) {
	// struct task_cam_socket_params const *params = p_param;
}
