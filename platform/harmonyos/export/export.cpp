/**************************************************************************/
/*  export.cpp - HarmonyOS Export Registration                             */
/**************************************************************************/

#include "export.h"

#include "export_plugin.h"

#include "core/object/class_db.h"
#include "editor/export/editor_export.h"

void register_harmonyos_exporter_types() {
	GDREGISTER_VIRTUAL_CLASS(EditorExportPlatformHarmonyOS);
}

void register_harmonyos_exporter() {
	Ref<EditorExportPlatformHarmonyOS> platform;
	platform.instantiate();
	platform->set_name("HarmonyOS");
	platform->set_os_name("HarmonyOS");

	EditorExport::get_singleton()->add_export_platform(platform);
}
