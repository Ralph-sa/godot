/**************************************************************************/
/*  export_plugin.h - HarmonyOS Export Plugin                              */
/**************************************************************************/

#pragma once

#include "editor/export/editor_export_platform_pc.h"

class EditorExportPlatformHarmonyOS : public EditorExportPlatformPC {
	GDCLASS(EditorExportPlatformHarmonyOS, EditorExportPlatformPC);

#if 0
	// TODO: Enable when run_icon is available
	Ref<ImageTexture> run_icon;
#endif


public:
	virtual Error export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags = 0, bool p_notify = true) override;
	virtual List<String> get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const override;
	virtual void get_export_options(List<ExportOption> *r_options) const override;
	virtual bool has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug = false) const override;
	virtual void get_platform_features(List<String> *r_features) const override;
	virtual String get_template_file_name(const String &p_target, const String &p_arch) const override;

	virtual Ref<Texture2D> get_run_icon() const override;
	virtual bool poll_export() override;
	virtual int get_options_count() const override;
	virtual String get_option_label(int p_index) const override;
	virtual String get_option_tooltip(int p_index) const override;
	virtual Error run(const Ref<EditorExportPreset> &p_preset, int p_device, BitField<EditorExportPlatform::DebugFlags> p_debug_flags) override;
	virtual void cleanup() override;

	virtual void initialize() override;
};
