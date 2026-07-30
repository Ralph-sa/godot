/**************************************************************************/
/*  export_plugin.cpp - HarmonyOS Export Plugin                            */
/**************************************************************************/

#include "export_plugin.h"

#include "logo_svg.gen.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/export/editor_export.h"
#include "editor/export/project_export.h"
#include "editor/themes/editor_scale.h"
#include "scene/resources/image_texture.h"

#include "modules/svg/image_loader_svg.h"

Error EditorExportPlatformHarmonyOS::_process_icon(const Ref<EditorExportPreset> &p_preset, const String &p_src_path, const String &p_dst_path) {
	// TODO: Implement HAP icon processing
	return OK;
}

Error EditorExportPlatformHarmonyOS::_add_data(const Ref<EditorExportPreset> &p_preset, const String &p_path) {
	// TODO: Implement HAP resource embedding
	return OK;
}

// HAP export via hvigor (external build tool).
// The Godot editor generates the project files (.pck, config),
// then hvigor assembles the final .hap package.
Error EditorExportPlatformHarmonyOS::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags, bool p_notify) {
	return ERR_UNAVAILABLE;
}

List<String> EditorExportPlatformHarmonyOS::get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const {
	List<String> list;
	list.push_back("hap");
	return list;
}

void EditorExportPlatformHarmonyOS::get_export_options(List<ExportOption> *r_options) const {
	EditorExportPlatformPC::get_export_options(r_options);

	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "binary_format/architecture", PROPERTY_HINT_ENUM, "arm64"), "arm64"));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "custom_template/debug"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "custom_template/release"), ""));
}

bool EditorExportPlatformHarmonyOS::has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug) const {
	bool valid = true;

	// Check architecture support
	String arch = p_preset->get("binary_format/architecture");
	if (arch != "arm64") {
		r_error += TTR("Only arm64 architecture is supported for HarmonyOS.") + "\n";
		valid = false;
	}

	// Check template existence
	String custom_debug = p_preset->get("custom_template/debug").operator String().strip_edges();
	String custom_release = p_preset->get("custom_template/release").operator String().strip_edges();

	if ((p_debug && custom_debug.is_empty()) || (!p_debug && custom_release.is_empty())) {
		r_missing_templates = true;
		valid = false;
	}

	return valid;
}

void EditorExportPlatformHarmonyOS::get_platform_features(List<String> *r_features) const {
	r_features->push_back("harmonyos");
	r_features->push_back("arm64");
	r_features->push_back("mobile");
	r_features->push_back("pc");
}

String EditorExportPlatformHarmonyOS::get_template_file_name(const String &p_target, const String &p_arch) const {
	return "harmonyos_" + p_target + ".hap";
}

Ref<Texture2D> EditorExportPlatformHarmonyOS::get_run_icon() const {
#if 0
	// TODO: Add run icon when available
	return run_icon;
#endif
	return Ref<Texture2D>();
}

bool EditorExportPlatformHarmonyOS::poll_export() {
	return false;
}

int EditorExportPlatformHarmonyOS::get_options_count() const {
	return 0;
}

String EditorExportPlatformHarmonyOS::get_option_label(int p_index) const {
	return EditorExportPlatformPC::get_option_label(p_index);
}

String EditorExportPlatformHarmonyOS::get_option_tooltip(int p_index) const {
	return EditorExportPlatformPC::get_option_tooltip(p_index);
}

Error EditorExportPlatformHarmonyOS::run(const Ref<EditorExportPreset> &p_preset, int p_device, BitField<EditorExportPlatform::DebugFlags> p_debug_flags) {
	// One-click deploy not yet supported on HarmonyOS
	return ERR_UNAVAILABLE;
}

void EditorExportPlatformHarmonyOS::cleanup() {
	// No remote cleanup needed
}

void EditorExportPlatformHarmonyOS::initialize() {
	if (EditorNode::get_singleton()) {
		Ref<Image> img = memnew(Image);
		const bool upsample = !Math::is_equal_approx(Math::round(EDSCALE), EDSCALE);

		ImageLoaderSVG::create_image_from_string(img, _harmonyos_logo_svg, EDSCALE, upsample, false);
		set_logo(ImageTexture::create_from_image(img));
	}
}
