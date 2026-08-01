/**************************************************************************/
/*  export_plugin.h                                                       */
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

#include "editor/export/editor_export_platform.h"
#include "scene/resources/image_texture.h"

/* EditorExportPlatformOHOS：鸿蒙导出平台（生成 .hap）。
 *
 * 对应 Android 的 EditorExportPlatformAndroid（生成 APK/AAB）。
 * OHOS 导出目标：DevEco 工程模板（deveco/ 目录）+ 引擎 .so 打包为 HAP。
 *
 * 第 1 轮（骨架期）：仅实现平台名称/logo/特性声明与空导出流程，
 * 打包导出（hap 组装 + hvigor 编译）在完善期实现。
 */
class EditorExportPlatformOHOS : public EditorExportPlatform {
	GDCLASS(EditorExportPlatformOHOS, EditorExportPlatform);

	Ref<ImageTexture> logo;
	Ref<ImageTexture> run_icon;

protected:
	static void _bind_methods();

public:
	virtual void get_platform_features(List<String> *r_features) const override;
	virtual void get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) const override;
	virtual void get_export_options(List<ExportOption> *r_options) const override;

	virtual String get_name() const override;
	virtual String get_os_name() const override;
	virtual Ref<Texture2D> get_logo() const override;

	virtual bool has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug = false) const override;
	virtual bool has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const override;

	virtual List<String> get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const override;
	virtual Error export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags = 0, bool p_notify = true) override;

	EditorExportPlatformOHOS();

private:
	// ---- 内部辅助（第 5 轮：导出流程） ----
	// 定位 DevEco 模板工程路径（编辑器设置优先，回退内置目录）
	String _get_templates_path() const;
	// 递归拷贝目录（模板 -> 输出工程）
	Error _copy_dir_recursive(const String &p_from, const String &p_to);
	// 改写 DevEco 工程 app.json5（bundleName/版本）
	void _rewrite_app_config(const String &p_project, const Ref<EditorExportPreset> &p_preset);
};
