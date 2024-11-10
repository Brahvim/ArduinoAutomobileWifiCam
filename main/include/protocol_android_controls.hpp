#pragma once

enum android_control_id {

	ANDROID_CONTROL_STEER, // `uint8_t`/`char` specifying how much power is put into wheels on each side!
	ANDROID_CONTROL_GEAR, // [`B`, `F`, `N`]!... `B` is backwards, `N` is "neutral" *a.k.a "stop"*, and `F` is forwards!
	ANDROID_CONTROL_MODE, // Use this parameter to send the car back into obstacle-avoidance mode.

};

enum android_gear_value {

	ANDROID_GEAR_BACKWARDS = 'B',
	ANDROID_GEAR_FORWARDS = 'F',
	ANDROID_GEAR_NEUTRAL = 'N',

};

static inline char const* g_android_controls_http_parameters[] = {

	"steer",
	"gear",
	"mode",

};
