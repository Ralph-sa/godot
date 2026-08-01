/**************************************************************************/
/*  os_ohos.h                                                             */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "core/input/input_event.h"
#include "core/templates/rb_map.h"
#include "drivers/unix/os_unix.h"

/* OS_OHOS：HarmonyOS 平台操作系统抽象。
 *
 * 继承 OS_Unix（OHOS 基于 musl libc，POSIX 语义与 Linux 一致）。
 * 第 1 轮（骨架期）：实现名称/版本、沙盒路径（filesDir/cacheDir）、
 * 生命周期（initialize/finalize/main_loop）等基础能力；
 * 后续轮次补全进程/字体/系统目录等桌面能力。
 *
 * 与 macOS 的差异：macOS 继承 OS_Unix 但用 CFRunLoop 驱动；
 * OHOS 由 ArkUI 主线程 + NAPI 回调驱动，引擎主循环由 main_ohos.cpp 显式迭代。
 */
class OS_OHOS : public OS_Unix {
	// 沙盒路径缓存（鸿蒙 App 沙盒内，filesDir/cacheDir）
	mutable String data_dir_cache;
	mutable String cache_dir_cache;
	mutable String temp_dir_cache;

	// 当前引擎主循环（由 main_ohos.cpp 设置）
	MainLoop *main_loop = nullptr;

	// 从 NAPI 桥注入的应用沙盒路径（main_ohos.cpp 启动时设置）
	String sandbox_files_dir;
	String sandbox_cache_dir;

protected:
	virtual void initialize_core() override;
	virtual void initialize() override;
	virtual void finalize() override;

	virtual void initialize_joypads() override;

	virtual void set_main_loop(MainLoop *p_main_loop) override;
	virtual void delete_main_loop() override;

public:
	static OS_OHOS *get_singleton();

	// ---- 名称/版本 ----
	virtual String get_name() const override;            // "HarmonyOS"
	virtual String get_distribution_name() const override;
	virtual String get_version() const override;
	virtual String get_version_alias() const override;

	virtual MainLoop *get_main_loop() const override;

	// ---- 沙盒路径（鸿蒙 App 沙盒模型） ----
	virtual String get_config_path() const override;
	virtual String get_data_path() const override;
	virtual String get_cache_path() const override;
	virtual String get_temp_path() const override;
	virtual String get_user_data_dir(const String &p_user_dir) const override;
	virtual String get_resource_dir() const override;
	virtual String get_executable_path() const override;
	virtual String get_locale() const override;
	virtual String get_processor_name() const override;
	virtual String get_model_name() const override;
	virtual String get_unique_id() const override;

	// ---- 系统交互（骨架期先给基础实现，后续补全） ----
	virtual Error shell_open(const String &p_uri) override;
	virtual String get_system_ca_certificates() override;
	virtual Error get_entropy(uint8_t *r_buffer, int p_bytes) override;

	// ---- 沙盒路径注入（由 NAPI 桥调用） ----
	void set_sandbox_paths(const String &p_files_dir, const String &p_cache_dir);

	virtual bool _check_internal_feature_support(const String &p_feature) override;

	OS_OHOS();
	~OS_OHOS();

	// 引擎主循环驱动（由 main_ohos.cpp 调用）
	bool main_loop_iterate();
	void main_loop_begin();
	void main_loop_end();
};
