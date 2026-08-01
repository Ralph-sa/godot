/**************************************************************************/
/*  export.cpp                                                            */
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

#include "export.h"

#include "export_plugin.h"

#include "core/object/class_db.h"
#include "core/os/os.h"
#include "editor/export/editor_export.h"
#include "editor/settings/editor_settings.h"

void register_ohos_exporter_types() {
	GDREGISTER_VIRTUAL_CLASS(EditorExportPlatformOHOS);
}

void register_ohos_exporter() {
	// 编辑器设置：DevEco SDK 路径（后续导出 .hap 时需要）
	EDITOR_DEF_BASIC("export/ohos/deveco_sdk_path", OS::get_singleton()->get_environment("DEVECO_SDK_HOME"));
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "export/ohos/deveco_sdk_path", PROPERTY_HINT_GLOBAL_DIR));

	EDITOR_DEF_BASIC("export/ohos/bundle_name", "com.example.godot_game");
	EDITOR_DEF_BASIC("export/ohos/version_code", 1);
	EDITOR_DEF_BASIC("export/ohos/version_name", "1.0.0");

	// 注册 OHOS 导出平台
	Ref<EditorExportPlatformOHOS> exporter = Ref<EditorExportPlatformOHOS>(memnew(EditorExportPlatformOHOS));
	EditorExport::get_singleton()->add_export_platform(exporter);
}
