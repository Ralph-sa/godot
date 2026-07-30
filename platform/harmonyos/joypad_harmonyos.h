/**************************************************************************/
/*  joypad_harmonyos.h - HarmonyOS Joypad Support                         */
/**************************************************************************/

#pragma once

#include "core/input/input.h"
#include "core/templates/hash_map.h"

class JoypadHarmonyOS {
	struct JoypadDevice {
		int id = -1;
		int fd = -1;
		String guid;
		String name;
		bool attached = false;
	};

	JoypadDevice joypads[Input::JOYPADS_MAX];
	HashMap<String, int> guid_to_joypad_id;
	bool initialized = false;

	void enumerate_devices();

public:
	~JoypadHarmonyOS();

	Error initialize();
	void process_events();
	void close_joypad(int p_pad_idx);

	// Device query methods delegated to Input singleton.
	static bool is_joy_known(int p_device);
	static String get_joy_guid(int p_device);
	static TypedArray<int> get_connected_joypads();
};
