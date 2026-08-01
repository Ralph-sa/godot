/**************************************************************************/
/*  export_plugin.cpp                                                     */
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

#include "export_plugin.h"

#include "core/object/class_db.h"
#include "core/string/ustring.h"

void EditorExportPlatformOHOS::_bind_methods() {
	// 骨架期：暂无需暴露的方法
}

EditorExportPlatformOHOS::EditorExportPlatformOHOS() {
	// 加载平台 logo（骨架期用空图占位，后续替换为正式 logo）
	Ref<Image> img = memnew(Image);
	logo = ImageTexture::create_from_image(img);
	run_icon = logo;
}

void EditorExportPlatformOHOS::get_platform_features(List<String> *r_features) const {
	r_features->push_back("ohos");
	r_features->push_back("mobile");
	r_features->push_back("arm64");
}

void EditorExportPlatformOHOS::get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) const {
	// 骨架期：预设特性与平台特性一致（后续按导出选项扩展）
	get_platform_features(r_features);
}

void EditorExportPlatformOHOS::get_export_options(List<ExportOption> *r_options) const {
	// 骨架期：导出选项（bundle 名称/版本等）
	// 完整导出选项在完善期实现
	ExportOption bundle_name = ExportOption(PropertyInfo(Variant::STRING, "bundle_name"), "com.example.godot_game");
	r_options->push_back(bundle_name);

	ExportOption version_name = ExportOption(PropertyInfo(Variant::STRING, "version_name"), "1.0.0");
	r_options->push_back(version_name);
}

String EditorExportPlatformOHOS::get_name() const {
	return "HarmonyOS";
}

String EditorExportPlatformOHOS::get_os_name() const {
	return "ohos";
}

Ref<Texture2D> EditorExportPlatformOHOS::get_logo() const {
	return logo;
}

bool EditorExportPlatformOHOS::has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug) const {
	// 骨架期：报告导出模板缺失（完整模板在完善期提供）
	r_missing_templates = true;
	r_error = "HarmonyOS export templates are not available yet.";
	return false;
}

bool EditorExportPlatformOHOS::has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const {
	r_error = "HarmonyOS project configuration is not available yet.";
	return false;
}

List<String> EditorExportPlatformOHOS::get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const {
	List<String> list;
	list.push_back("hap");
	return list;
}

Error EditorExportPlatformOHOS::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags, bool p_notify) {
	// 骨架期：导出流程未实现（完善期通过 deveco/ 工程模板 + hvigor 组装 HAP）
	return ERR_UNAVAILABLE;
}
