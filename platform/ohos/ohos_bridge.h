/**************************************************************************/
/*  ohos_bridge.h                                                         */
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

#include "core/object/object.h"
#include "core/string/ustring.h"
#include "core/variant/callable.h"

/* OHOS NAPI 桥接口。
 *
 * 供平台各子系统（DisplayServer/OS）调用由 main_ohos.cpp 实现的
 * NAPI 跨层能力（剪贴板/文件对话框/窗口操作等）。实现均位于
 * main_ohos.cpp，经 ArkTS 侧注册的 @ohos.pasteboard 等系统能力桥接。
 *
 * 第 5 轮：剪贴板文本读写（编辑器复制/粘贴必需）+ 系统文件选择器。
 */

// 设置剪贴板文本（对应 macOS NSPasteboard setString / @ohos.pasteboard）
void ohos_clipboard_set_text(const String &p_text);

// 读取剪贴板文本（@ohos.pasteboard getPasteData，无内容返回空串）
String ohos_clipboard_get_text();

// 弹出系统文件选择器（@ohos.file.picker DocumentViewPicker）。
// p_mode 为 DisplayServerEnums::FileDialogMode（打开/保存等）。
// 选择完成后在引擎线程调用 p_callback（参数为 PackedStringArray 路径列表；
// 用户取消时为空数组）。立即返回 OK（异步）。
Error ohos_pick_files(const String &p_title, int p_mode, const Callable &p_callback);

// 请求窗口模式切换（全屏/最大化/窗口化）。
// p_mode 为 DisplayServerEnums::WindowMode；经 ArkTS @ohos.window 应用。
void ohos_window_set_mode(int p_mode);

// 请求窗口置顶（悬浮窗，第 5 轮：编辑器预览用）
void ohos_window_set_always_on_top(bool p_enabled);

// 从 HAP rawfile 提取文件到沙盒（第 6 轮：导出的 main.pck）。
// p_name 为 rawfile 内相对路径；p_dest 为目标沙盒路径。
// 文件不存在返回 ERR_FILE_NOT_FOUND；资源管理器未初始化返回 ERR_UNAVAILABLE。
Error ohos_extract_raw_file(const String &p_name, const String &p_dest);

// ---- 子窗口桥（第 7 轮：@ohos.window createWindow） ----
// 请求 ArkTS 创建/销毁/调整原生子窗口（编辑器子窗口：弹窗/工具面板）。
void ohos_subwindow_create(int p_id, int p_x, int p_y, int p_w, int p_h);
void ohos_subwindow_destroy(int p_id);
void ohos_subwindow_set_title(int p_id, const String &p_title);
void ohos_subwindow_set_rect(int p_id, int p_x, int p_y, int p_w, int p_h);
void ohos_subwindow_set_visible(int p_id, bool p_visible);

// ---- 指针可见性桥（第 7 轮：@ohos.multimodalInput.pointer） ----
// 鼠标捕获/隐藏（对应 macOS CGDisplayHideCursor / CGAssociateMouseAndMouseCursorPosition）
void ohos_mouse_set_visible(bool p_visible);
