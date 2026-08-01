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

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/string/ustring.h"
#include "editor/export/editor_export.h"
#include "editor/settings/editor_settings.h"

void EditorExportPlatformOHOS::_bind_methods() {
	// 绑定导出方法（EditorExportPlatform 基类已绑定，此处无新增方法）
}

EditorExportPlatformOHOS::EditorExportPlatformOHOS() {
	// 平台 logo（骨架期用空图占位，后续替换为正式 logo）
	Ref<Image> img = memnew(Image);
	logo = ImageTexture::create_from_image(img);
	run_icon = logo;
}

void EditorExportPlatformOHOS::get_platform_features(List<String> *r_features) const {
	// 平台特性：ohos + mobile + arm64（导出器特性检测用）
	r_features->push_back("ohos");
	r_features->push_back("mobile");
	r_features->push_back("arm64");
}

void EditorExportPlatformOHOS::get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) const {
	// 预设特性：平台特性 + 按导出选项扩展
	get_platform_features(r_features);

	String arch = p_preset->get("arch");
	if (arch == "arm64") {
		r_features->push_back("arm64");
	}
}

void EditorExportPlatformOHOS::get_export_options(List<ExportOption> *r_options) const {
	// 导出选项（对应 DevEco 工程 app.json5 配置）
	// 应用 bundle 名称（对应 application_id / app.json5 bundleName）
	ExportOption bundle_name = ExportOption(PropertyInfo(Variant::STRING, "bundle_name"), "com.example.godot_game");
	r_options->push_back(bundle_name);

	// 应用显示名称
	ExportOption app_name = ExportOption(PropertyInfo(Variant::STRING, "app_name"), "Godot Game");
	r_options->push_back(app_name);

	// 版本号（整数，安装包版本递增）
	ExportOption version_code = ExportOption(PropertyInfo(Variant::INT, "version_code"), 1);
	r_options->push_back(version_code);

	// 版本名（展示用，如 1.0.0）
	ExportOption version_name = ExportOption(PropertyInfo(Variant::STRING, "version_name"), "1.0.0");
	r_options->push_back(version_name);

	// 目标 CPU 架构（当前仅 arm64，后续追加 x86_64 供模拟器）
	ExportOption arch = ExportOption(PropertyInfo(Variant::STRING, "arch", PROPERTY_HINT_ENUM, "arm64"), "arm64");
	r_options->push_back(arch);

	// 屏幕方向（PC/2in1 默认 auto）
	ExportOption orientation = ExportOption(PropertyInfo(Variant::STRING, "orientation", PROPERTY_HINT_ENUM, "auto,landscape,portrait"), "auto");
	r_options->push_back(orientation);

	// 是否将 pck 打包进 rawfile（HAP 内嵌，默认开启）
	ExportOption include_pck = ExportOption(PropertyInfo(Variant::BOOL, "include_pck_in_rawfile"), true);
	r_options->push_back(include_pck);
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
	// 校验导出配置：检查 DevEco 模板目录是否存在
	String templates_path = _get_templates_path();
	r_missing_templates = true;
	if (templates_path.is_empty() || !DirAccess::exists(templates_path)) {
		r_error = "HarmonyOS export templates not found. Set 'export/ohos/deveco_sdk_path' in Editor Settings to the DevEco template directory.";
		return false;
	}
	r_missing_templates = false;
	return true;
}

bool EditorExportPlatformOHOS::has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const {
	// 校验项目配置：bundle_name 必须合法（包名规范）
	String bundle_name = p_preset->get("bundle_name");
	if (bundle_name.is_empty() || bundle_name.contains(" ")) {
		r_error = "Invalid bundle_name: must be a valid package name without spaces.";
		return false;
	}
	return true;
}

List<String> EditorExportPlatformOHOS::get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const {
	List<String> list;
	list.push_back("hap");
	return list;
}

Error EditorExportPlatformOHOS::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags, bool p_notify) {
	// 导出流程：从 DevEco 模板工程生成可构建的 HAP 工程
	// 步骤：
	//   1. 定位模板工程（export/ohos/deveco_sdk_path）；
	//   2. 拷贝模板到输出目录；
	//   3. 生成 pck 并放入 entry/src/main/resources/rawfile/；
	//   4. 根据导出选项改写 app.json5（bundleName/版本）；
	//   5. 用户用 DevEco Studio 打开工程构建 HAP。
	(void)p_notify; // 导出完成通知由编辑器导出管理器统一处理

	String templates_path = _get_templates_path();
	if (templates_path.is_empty() || !DirAccess::exists(templates_path)) {
		// 编辑器端已通过 has_valid_export_configuration 校验，此处兜底
		return ERR_FILE_NOT_FOUND;
	}

	// 输出目录：p_path 为 .hap 目标，生成同名工程目录
	String out_project = p_path.get_base_dir().path_join(p_path.get_file().get_basename());

	// 1. 拷贝模板工程到输出目录（先清空旧目录）
	Error err = _copy_dir_recursive(templates_path, out_project);
	if (err != OK) {
		return err;
	}

	// 2. 生成 pck（导出资源打包）
	String rawfile_dir = out_project.path_join("entry").path_join("src").path_join("main").path_join("resources").path_join("rawfile");
	DirAccess::make_dir_recursive_absolute(rawfile_dir);
	String pck_path = rawfile_dir.path_join("main.pck");
	err = save_pack(p_preset, p_debug, pck_path);
	if (err != OK) {
		return err;
	}

	// 3. 改写 app.json5（bundleName / 版本号）
	_rewrite_app_config(out_project, p_preset);

	// 4. 写入原始资源（pck 内嵌引用路径）
	// pck 打包时主场景路径已在 pck 中，无需额外配置

	return OK;
}

// ---- 内部辅助 ----

String EditorExportPlatformOHOS::_get_templates_path() const {
	// 模板路径：优先编辑器设置，回退内置模板目录
	String settings_path = EditorSettings::get_singleton()->get("export/ohos/deveco_sdk_path");
	if (!settings_path.is_empty() && DirAccess::exists(settings_path)) {
		return settings_path;
	}
	// 内置模板：相对 Godot 可执行目录（发布时随引擎分发）
	String builtin = OS::get_singleton()->get_executable_path().get_base_dir().path_join("ohos_templates");
	return builtin;
}

Error EditorExportPlatformOHOS::_copy_dir_recursive(const String &p_from, const String &p_to) {
	// 递归拷贝目录（模板 -> 输出工程）
	DirAccess::make_dir_recursive_absolute(p_to);
	Ref<DirAccess> src = DirAccess::open(p_from);
	if (src.is_null()) {
		return ERR_CANT_OPEN;
	}
	src->list_dir_begin();
	String cur = src->get_next();
	while (!cur.is_empty()) {
		if (cur == "." || cur == "..") {
			cur = src->get_next();
			continue;
		}
		String src_path = p_from.path_join(cur);
		String dst_path = p_to.path_join(cur);
		if (src->current_is_dir()) {
			Error err = _copy_dir_recursive(src_path, dst_path);
			if (err != OK) {
				return err;
			}
		} else {
			// 拷贝文件
			Ref<FileAccess> in = FileAccess::open(src_path, FileAccess::READ);
			if (in.is_null()) {
				return ERR_CANT_OPEN;
			}
			Ref<FileAccess> out = FileAccess::open(dst_path, FileAccess::WRITE);
			if (out.is_null()) {
				return ERR_CANT_OPEN;
			}
			Vector<uint8_t> data = in->get_buffer(in->get_length());
			out->store_buffer(data);
			out->close();
			in->close();
		}
		cur = src->get_next();
	}
	src->list_dir_end();
	return OK;
}

void EditorExportPlatformOHOS::_rewrite_app_config(const String &p_project, const Ref<EditorExportPreset> &p_preset) {
	// 改写 DevEco 工程 app.json5：bundleName / 版本
	String app_config_path = p_project.path_join("AppScope").path_join("app.json5");
	if (!FileAccess::exists(app_config_path)) {
		return;
	}
	String bundle_name = p_preset->get("bundle_name");
	int version_code = p_preset->get("version_code");
	String version_name = p_preset->get("version_name");

	String content = FileAccess::get_file_as_string(app_config_path);
	content = content.replace("\"com.example.godot_game\"", "\"" + bundle_name + "\"");
	content = content.replace("\"1.0.0\"", "\"" + version_name + "\"");

	// 版本号（code）替换：匹配 "code": 数字
	// 简化处理：查找 versionCode 键
	if (content.contains("versionCode")) {
		content = content.replace("versionCode\": 1", "versionCode\": " + itos(version_code));
	}

	Ref<FileAccess> f = FileAccess::open(app_config_path, FileAccess::WRITE);
	if (f.is_valid()) {
		f->store_string(content);
		f->close();
	}
}
