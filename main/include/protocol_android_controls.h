#pragma once

#define ANDROID_HTTP_CONTENT_TYPE             "application/octet-stream"

enum android_control_id {

    ANDROID_BUTTON_ID_STEER,
    ANDROID_BUTTON_ID_FORWARD,
    ANDROID_BUTTON_ID_BACKWARD,

};

enum android_button_events {

    PROTOCOL_ANDROID_CONTROLS_BUTTON_EVENT_PRESSED,
    PROTOCOL_ANDROID_CONTROLS_BUTTON_EVENT_RELEASED,

};

