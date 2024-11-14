// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <esp_log.h>
#include <esp_timer.h>
#include <esp_camera.h>
#include <esp_http_server.h>

#include <sdkconfig.h>
#include <img_converters.h>

extern httpd_uri_t g_uri_controls;
httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

#define PART_BOUNDARY "123456789000000000000987654321"

static const char *TAG = __FILE__;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %d.%06d\r\n\r\n";

struct ra_filter {

	size_t size;  // Number of values used for filtering.
	size_t index; // Current value index...
	size_t count; // Value-count...

	int sum;
	int *values; // Array to be filled with values.

};

static struct ra_filter s_ra_filter;

static struct ra_filter *ra_filter_init(struct ra_filter *const p_filter, size_t const p_sample_size) {
	memset(p_filter, 0, sizeof(struct ra_filter));
	p_filter->values = (int*) malloc(p_sample_size * sizeof(int));

	if (!p_filter->values) {
		return NULL;
	}

	memset(p_filter->values, 0, p_sample_size * sizeof(int));
	p_filter->size = p_sample_size;

	return p_filter;
}

static int ra_filter_run(struct ra_filter *const p_filter, int const value) {
	if (!p_filter->values) {
		return value;
	}

	p_filter->sum -= p_filter->values[p_filter->index];
	p_filter->values[p_filter->index] = value;
	p_filter->sum += p_filter->values[p_filter->index];
	p_filter->index++;
	p_filter->index = p_filter->index % p_filter->size;

	if (p_filter->count < p_filter->size) {
		p_filter->count++;
	}

	return p_filter->sum / p_filter->count;
}

static esp_err_t stream_handler(httpd_req_t *const p_request) {
	static int64_t last_frame = 0;
	static char *part_buf[128];

	struct timeval timestamp;
	camera_fb_t *fb = NULL;
	esp_err_t err = ESP_OK;

	size_t jpg_buf_len = 0;
	uint8_t *jpg_buf = NULL;

	if (!last_frame) {
		last_frame = esp_timer_get_time();
	}

	err = httpd_resp_set_type(p_request, _STREAM_CONTENT_TYPE);
	if (err != ESP_OK) {
		return err;
	}

	httpd_resp_set_hdr(p_request, "Access-Control-Allow-Origin", "*");
	httpd_resp_set_hdr(p_request, "X-Framerate", "60");

	while (true) {
		fb = esp_camera_fb_get();

		if (!fb) {

			ESP_LOGE(TAG, "Camera capture failed!...");
			err = ESP_FAIL;

		} else {

			timestamp.tv_usec = fb->timestamp.tv_usec;
			timestamp.tv_sec = fb->timestamp.tv_sec;

			if (fb->format != PIXFORMAT_JPEG) {

				bool jpeg_converted = frame2jpg(fb, 80, &jpg_buf, &jpg_buf_len);
				esp_camera_fb_return(fb);
				fb = NULL;

				if (!jpeg_converted) {
					ESP_LOGE(TAG, "JPEG compression failed");
					err = ESP_FAIL;
				}

			} else {

				jpg_buf_len = fb->len;
				jpg_buf = fb->buf;

			}

		}

		if (err == ESP_OK) {
			err = httpd_resp_send_chunk(p_request, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
		}

		if (err == ESP_OK) {
			size_t const hlen = snprintf((char*) part_buf, 128, _STREAM_PART, jpg_buf_len, timestamp.tv_sec, timestamp.tv_usec);
			err = httpd_resp_send_chunk(p_request, (const char*) part_buf, hlen);
		}

		if (err == ESP_OK) {
			err = httpd_resp_send_chunk(p_request, (const char*) jpg_buf, jpg_buf_len);
		}

		if (fb) {

			esp_camera_fb_return(fb);
			fb = NULL;
			jpg_buf = NULL;

		} else if (jpg_buf) {

			free(jpg_buf);
			jpg_buf = NULL;

		}

		if (err != ESP_OK) {

			ESP_LOGE(TAG, "Send frame failed");
			break;

		}

		// int64_t const frame_end = esp_timer_get_time();
		// int64_t const frame_time = (frame_end - last_frame) / 1000;
		// uint32_t const avg_frame_time = ra_filter_run(&s_ra_filter, frame_time);

		// ESP_LOGI(
		// 	TAG,
		// 	"MJPG: %luB %lums (%.1ffps), AVG: %lums (%.1ffps)",
		// 	(uint32_t) (jpg_buf_len),
		// 	(uint32_t) frame_time,
		// 	1000.0 / (uint32_t) frame_time,
		// 	avg_frame_time,
		// 	1000.0 / avg_frame_time
		// );

	}

	return err;
}

void startCameraServer() {
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	// config.max_uri_handlers = 16;
	config.max_uri_handlers = 2;

	httpd_uri_t stream_uri = {

		.uri = "/stream",
		.method = HTTP_GET,
		.handler = stream_handler,
		.user_ctx = NULL,
#ifdef CONFIG_HTTPD_WS_SUPPORT
		.is_websocket = true,
		.handle_ws_control_frames = false,
		.supported_subprotocol = NULL
#endif

	};

	ra_filter_init(&s_ra_filter, 20);

	ESP_LOGI(TAG, "Starting web server on port: '%d'", config.server_port);
	if (httpd_start(&camera_httpd, &config) == ESP_OK) {
		httpd_register_uri_handler(camera_httpd, &g_uri_controls);
	}

	config.ctrl_port += 1;
	config.server_port += 1;
	ESP_LOGI(TAG, "Starting stream server on port: '%d'", config.server_port);

	if (httpd_start(&stream_httpd, &config) == ESP_OK) {
		httpd_register_uri_handler(stream_httpd, &stream_uri);
	}
}
