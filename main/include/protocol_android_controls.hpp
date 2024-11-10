#pragma once

enum android_control_id {

	ANDROID_CONTROL_STEER, // `uint8_t`/`char` specifying how much power is put into wheels on each side!
	ANDROID_CONTROL_GEAR, // [-1, 0, 1]!... `-1` is backwards, `0` is "neutral" *a.k.a "stop"*, and `1` is forwards!

};

enum android_gear_value {

	ANDROID_GEAR_BACKWARDS = -1,
	ANDROID_GEAR_FORWARDS = 0,
	ANDROID_GEAR_NEUTRAL = 1,

};

static inline char const* g_android_controls_http_parameters[] = {

	"steer"
	"gear",

};
