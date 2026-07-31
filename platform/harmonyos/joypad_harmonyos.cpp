/**************************************************************************/
/*  joypad_harmonyos.cpp - HarmonyOS Joypad Support                       */
/**************************************************************************/

#include "joypad_harmonyos.h"

#ifdef HARMONYOS_ENABLED
#include "harmonyos_log.h"
#endif

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>

// Linux joystick API structures (self-contained, no kernel header dependency)
struct js_event {
	uint32_t time;
	int16_t value;
	uint8_t type;
	uint8_t number;
};

#define JS_EVENT_BUTTON 0x01
#define JS_EVENT_AXIS 0x02
#define JS_EVENT_INIT 0x80

JoypadHarmonyOS::~JoypadHarmonyOS() {
	for (int i = 0; i < Input::JOYPADS_MAX; i++) {
		if (joypads[i].attached) {
			close_joypad(i);
		}
	}
}

Error JoypadHarmonyOS::initialize() {
	if (initialized) {
		return OK;
	}

	for (int i = 0; i < Input::JOYPADS_MAX; i++) {
		joypads[i].fd = -1;
	}

	enumerate_devices();
	initialized = true;

#ifdef HARMONYOS_ENABLED
	OH_LOG_INFO(LOG_APP, "JoypadHarmonyOS: initialized with %{public}d devices", guid_to_joypad_id.size());
#endif
	return OK;
}

void JoypadHarmonyOS::enumerate_devices() {
	DIR *dir = opendir("/dev/input");
	if (!dir) {
		return; // No /dev/input directory — no joysticks available
	}

	struct dirent *entry;
	while ((entry = readdir(dir)) != nullptr) {
		if (strncmp(entry->d_name, "js", 2) != 0) {
			continue;
		}

		char path[64];
		snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);

		int fd = open(path, O_RDONLY | O_NONBLOCK);
		if (fd < 0) {
			continue; // Cannot open this device
		}

		int joy_id = Input::get_singleton()->get_unused_joy_id();
		if (joy_id == -1) {
			close(fd);
			break; // No more slots available
		}

		joypads[joy_id].id = joy_id;
		joypads[joy_id].fd = fd;
		joypads[joy_id].attached = true;

		// Generate a simple GUID from the device path
		char guid_buf[64];
		snprintf(guid_buf, sizeof(guid_buf), "harmonyos_%s", entry->d_name);
		joypads[joy_id].guid = String(guid_buf);
		joypads[joy_id].name = String("Joystick ") + entry->d_name;

		guid_to_joypad_id.insert(joypads[joy_id].guid, joy_id);

		Dictionary joypad_info;
		joypad_info["raw_name"] = joypads[joy_id].name;

		Input::get_singleton()->joy_connection_changed(
				joy_id,
				true,
				joypads[joy_id].name,
				joypads[joy_id].guid,
				joypad_info);

#ifdef HARMONYOS_ENABLED
		OH_LOG_INFO(LOG_APP, "JoypadHarmonyOS: device %{public}s connected (id=%{public}d)",
				entry->d_name, joy_id);
#endif
	}

	closedir(dir);
}

void JoypadHarmonyOS::process_events() {
	for (int i = 0; i < Input::JOYPADS_MAX; i++) {
		if (!joypads[i].attached || joypads[i].fd < 0) {
			continue;
		}

		js_event ev;
		ssize_t n;

		while ((n = read(joypads[i].fd, &ev, sizeof(ev))) == sizeof(ev)) {
			ev.type &= ~JS_EVENT_INIT;

			if (ev.type == JS_EVENT_BUTTON) {
				Input::get_singleton()->joy_button(
						i,
						static_cast<JoyButton>(ev.number),
						ev.value != 0);
			} else if (ev.type == JS_EVENT_AXIS) {
				// Convert from int16 [INT16_MIN, INT16_MAX] to float [-1.0, 1.0]
				float value = (ev.value >= 0)
						? static_cast<float>(ev.value) / 32767.0f
						: static_cast<float>(ev.value) / 32768.0f;
				Input::get_singleton()->joy_axis(
						i,
						static_cast<JoyAxis>(ev.number),
						value);
			}
		}
	}
}

void JoypadHarmonyOS::close_joypad(int p_pad_idx) {
	if (p_pad_idx < 0 || p_pad_idx >= Input::JOYPADS_MAX) {
		return;
	}

	JoypadDevice &dev = joypads[p_pad_idx];
	if (!dev.attached) {
		return;
	}

	if (dev.fd >= 0) {
		close(dev.fd);
		dev.fd = -1;
	}

	Input::get_singleton()->joy_connection_changed(p_pad_idx, false, "");
	guid_to_joypad_id.erase(dev.guid);

	dev.attached = false;
	dev.id = -1;
	dev.guid = "";
	dev.name = "";
}

bool JoypadHarmonyOS::is_joy_known(int p_device) {
	if (p_device < 0 || p_device >= Input::JOYPADS_MAX) {
		return false;
	}
	return Input::get_singleton()->is_joy_known(p_device);
}

String JoypadHarmonyOS::get_joy_guid(int p_device) {
	if (p_device < 0 || p_device >= Input::JOYPADS_MAX) {
		return "";
	}
	return Input::get_singleton()->get_joy_guid(p_device);
}

TypedArray<int> JoypadHarmonyOS::get_connected_joypads() {
	return Input::get_singleton()->get_connected_joypads();
}
