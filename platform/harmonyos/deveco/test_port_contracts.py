#!/usr/bin/env python3
"""Focused static contracts for the HarmonyOS editor port."""

import re
import unittest
import zipfile
from pathlib import Path


DEVECO_ROOT = Path(__file__).resolve().parent
HARMONYOS_ROOT = DEVECO_ROOT.parent


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="strict")


def extract_block(source: str, marker: str) -> str:
    marker_pos = source.find(marker)
    if marker_pos < 0:
        raise AssertionError(f"marker not found: {marker}")
    brace_pos = source.find("{", marker_pos)
    if brace_pos < 0:
        raise AssertionError(f"opening brace not found after: {marker}")
    depth = 0
    for index in range(brace_pos, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[marker_pos:index + 1]
    raise AssertionError(f"closing brace not found after: {marker}")


class HarmonyOSPortContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.page = read(DEVECO_ROOT / "entry/src/main/ets/pages/GodotEditorPage.ets")
        cls.surface = read(DEVECO_ROOT / "entry/src/main/ets/components/GodotSurface.ets")
        cls.selector = read(DEVECO_ROOT / "entry/src/main/ets/components/ProjectSelector.ets")
        cls.vm = read(DEVECO_ROOT / "entry/src/main/ets/viewmodel/GodotEditorVM.ets")
        cls.module_json = read(DEVECO_ROOT / "entry/src/main/module.json5")
        cls.bridge = read(DEVECO_ROOT / "entry/src/main/cpp/godot_napi_bridge.cpp")
        cls.types = read(DEVECO_ROOT / "entry/src/main/cpp/types/libgodot_napi/Index.d.ts")
        cls.main_h = read(HARMONYOS_ROOT / "harmonyos_main.h")
        cls.main_cpp = read(HARMONYOS_ROOT / "harmonyos_main.cpp")
        cls.os_h = read(HARMONYOS_ROOT / "os_harmonyos.h")
        cls.os_cpp = read(HARMONYOS_ROOT / "os_harmonyos.cpp")
        cls.dir_cpp = read(HARMONYOS_ROOT / "dir_access_harmonyos.cpp")
        cls.input_cpp = read(HARMONYOS_ROOT / "harmonyos_input.cpp")
        cls.display_cpp = read(HARMONYOS_ROOT / "display_server_harmonyos.cpp")
        cls.vulkan_driver_cpp = read(
            HARMONYOS_ROOT.parents[1]
            / "drivers/vulkan/rendering_device_driver_vulkan.cpp"
        )
        cls.native_window_h = read(HARMONYOS_ROOT / "harmonyos_native_window.h")
        cls.cmake = read(DEVECO_ROOT / "entry/src/main/cpp/CMakeLists.txt")
        cls.build_profile = read(DEVECO_ROOT / "entry/build-profile.json5")
        cls.build_script = read(
            HARMONYOS_ROOT.parents[2]
            / ".cursor/skills/harmonyos-tools/scripts/build_project.sh"
        )
        cls.smoke_script = read(
            HARMONYOS_ROOT.parents[2]
            / ".cursor/skills/harmonyos-tools/scripts/run_smoke.sh"
        )

    def test_project_path_and_surface_reach_main_setup(self) -> None:
        selected = extract_block(self.page, "onProjectSelected =")
        self.assertNotIn("initEngine(", selected)
        surface_ready = extract_block(self.page, "onSurfaceReady =")
        self.assertIn(
            "this.viewModel.initEngine(this.projectPath, ctx.filesDir, ctx.cacheDir, ctx.tempDir,",
            surface_ready,
        )

        self.assertIn("godot_napi.loadLibrary(path, filesDir, cacheDir, tempDir,", self.vm)
        self.assertRegex(
            self.types,
            r"loadLibrary\s*:\s*\(projectPath:\s*string,\s*filesDir:\s*string,\s*"
            r"cacheDir:\s*string,\s*tempDir:\s*string,\s*surfaceId:\s*string,\s*"
            r"width:\s*number,\s*height:\s*number,\s*generation:\s*number\)\s*=>\s*number",
        )
        self.assertIn(
            "typedef int (*godot_init_t)(const char *, const char *, const char *, const char *, const char *, int, int,",
            self.bridge,
        )
        self.assertIn("unsigned long long, void *);", self.bridge)

        load_library = extract_block(self.bridge, "static napi_value NAPI_LoadLibrary")
        self.assertIn("argc < 8", load_library)
        self.assertIn("worker_args->project_path = project_path", load_library)
        self.assertIn("worker_args->files_dir = files_dir", load_library)
        self.assertIn("worker_args->cache_dir = cache_dir", load_library)
        self.assertIn("worker_args->temp_dir = temp_dir", load_library)
        self.assertIn("worker_args->surface_id = surface_id", load_library)
        self.assertIn("worker_args->surface_width", load_library)
        self.assertIn("worker_args->surface_height", load_library)
        self.assertIn("worker_args->surface_generation", load_library)
        self.assertIn("if (xcomponent == nullptr)", load_library)

        worker = extract_block(self.bridge, "static void *init_worker")
        self.assertIn("OH_NativeXComponent *xcomponent = get_current_xcomponent()", worker)
        self.assertIn("files_dir.c_str(), cache_dir.c_str(), temp_dir.c_str()", worker)
        self.assertIn(
            "surface_id.c_str(), surface_width, surface_height, surface_generation, xcomponent",
            worker,
        )

        self.assertRegex(
            self.main_h,
            r"int harmonyos_godot_init\(const char \*project_path, const char \*files_dir, "
            r"const char \*cache_dir,\s*const char \*temp_dir, const char \*surface_id, "
            r"int surface_width, int surface_height,\s*unsigned long long surface_generation, "
            r"void \*native_xcomponent\);",
        )
        init = extract_block(self.main_cpp, "HARMONYOS_EXPORT_FN int harmonyos_godot_init")
        configure_pos = init.index("configure_sandbox_paths(files_dir, cache_dir, temp_dir)")
        surface_pos = init.index("_apply_surface_created_on_engine_thread")
        setup_pos = init.index("Error err = Main::setup")
        self.assertLess(configure_pos, surface_pos)
        self.assertLess(surface_pos, setup_pos)
        self.assertLess(init.index('arg_strings.push_back("--path")'), setup_pos)

        constructor = extract_block(
            self.display_cpp,
            "DisplayServerHarmonyOS::DisplayServerHarmonyOS",
        )
        self.assertIn(
            "rendering_device->initialize(ctx, DisplayServerEnums::MAIN_WINDOW_ID)",
            constructor,
        )
        self.assertIn(
            "rendering_device->screen_create(DisplayServerEnums::MAIN_WINDOW_ID)",
            constructor,
        )
        self.assertLess(
            constructor.index("rendering_device->screen_create"),
            constructor.index("RendererCompositorRD::make_current"),
        )
        self.assertNotIn(
            "rendering_device->initialize(ctx, DisplayServerEnums::INVALID_WINDOW_ID)",
            constructor,
        )
        self.assertIn(
            "initialize_with_surface_id(uint64_t p_surface_id, uint64_t p_width, uint64_t p_height)",
            self.native_window_h,
        )
        self.assertIn("libraryname: 'godot_napi'", self.surface)
        self.assertNotIn("harmonyos_get_xcomponent", self.main_cpp)
        self.assertNotIn("harmonyos_get_xcomponent", self.bridge)

    def test_sandbox_paths_and_debug_symbols(self) -> None:
        self.assertIn("virtual String get_temp_path() const override", self.os_h)
        self.assertIn("get_data_path().path_join(p_user_dir)", self.os_cpp)
        self.assertIn("DirAccessHarmonyOS::configure_sandbox_roots", self.os_cpp)
        self.assertNotIn("_remap_to_sandbox", self.dir_cpp)
        self.assertIn("ERR_UNAUTHORIZED", self.dir_cpp)
        self.assertIn("-g3", self.cmake)
        self.assertIn("-O0", self.cmake)
        self.assertIn("-fno-omit-frame-pointer", self.cmake)
        self.assertIn("-Wl,--build-id=sha1", self.cmake)
        self.assertRegex(self.build_profile, r'"debugSymbol"\s*:\s*\{\s*"strip"\s*:\s*false')

    def test_recursive_sandbox_creation_accepts_existing_ancestors_only(self) -> None:
        strict_ancestor = extract_block(
            self.dir_cpp,
            "static bool _path_is_strict_ancestor_of_root",
        )
        self.assertIn('p_path == "/"', strict_ancestor)
        self.assertIn('p_root.begins_with(p_path + "/")', strict_ancestor)

        ancestor = extract_block(
            self.dir_cpp,
            "bool DirAccessHarmonyOS::is_sandbox_root_ancestor",
        )
        self.assertIn('_path_is_strict_ancestor_of_root', ancestor)
        self.assertNotIn("DirAccessUnix::make_dir", ancestor)

        make_dir = extract_block(self.dir_cpp, "Error DirAccessHarmonyOS::make_dir")
        existing_ancestor = (
            "is_sandbox_root_ancestor(resolved) && "
            "DirAccessUnix::dir_exists(resolved)"
        )
        self.assertIn(existing_ancestor, make_dir)
        self.assertIn("return ERR_ALREADY_EXISTS", make_dir)
        self.assertLess(
            make_dir.index(existing_ancestor),
            make_dir.index("_open_sandbox_parent"),
        )
        self.assertIn("mkdirat(", make_dir)
        self.assertNotIn("DirAccessUnix::make_dir(resolved)", make_dir)

        recursive = extract_block(
            self.dir_cpp,
            "Error DirAccessHarmonyOS::make_dir_recursive",
        )
        self.assertIn("_open_sandbox_root", recursive)
        self.assertIn("mkdirat(", recursive)
        self.assertIn("openat(", recursive)
        self.assertIn("O_NOFOLLOW", recursive)
        self.assertNotIn("DirAccessUnix::make_dir_recursive(resolved)", recursive)

    def test_clipboard_length_excludes_harmonyos_nul_terminator(self) -> None:
        clipboard_get = extract_block(
            self.display_cpp,
            "String DisplayServerHarmonyOS::clipboard_get",
        )
        self.assertIn("memchr(data, '\\0', len)", clipboard_get)
        self.assertIn("String::utf8(data, (int)text_len)", clipboard_get)
        self.assertNotIn("String::utf8(data, len)", clipboard_get)
        self.assertNotIn("ohos.permission.READ_PASTEBOARD", self.module_json)

    def test_empty_pipeline_cache_uses_null_initial_data_on_harmonyos(self) -> None:
        pipeline_cache = extract_block(
            self.vulkan_driver_cpp,
            "bool RenderingDeviceDriverVulkan::pipeline_cache_create",
        )
        self.assertIn("#ifdef HARMONYOS_ENABLED", pipeline_cache)
        self.assertIn("__x86_64__", pipeline_cache)
        self.assertIn("simulator_cache_data", pipeline_cache)
        self.assertIn("cache_info.initialDataSize > 0", pipeline_cache)
        self.assertIn(": nullptr", pipeline_cache)

    def test_project_import_is_atomic_and_sandbox_only(self) -> None:
        self.assertIn("picker.DocumentSelectMode.FOLDER", self.selector)
        self.assertIn("private isSafeEntryName", self.selector)
        self.assertIn("private isSandboxProjectPath", self.selector)
        self.assertIn("stagingLeaf = destinationLeaf + '.importing'", self.selector)
        self.assertIn("private assertSandboxDirectoryChain", self.selector)
        self.assertIn("private ensureSandboxProjectsDir", self.selector)
        self.assertIn("fileIo.mkdirSync(stagingDestination, false)", self.selector)
        self.assertIn("this.assertSandboxDirectoryChain(stagingDestination, false)", self.selector)
        import_flow = extract_block(self.selector, "private importAndOpenProject")
        self.assertIn(
            "godot_napi.commitProjectImport(",
            import_flow,
        )
        self.assertIn(
            "this.getCtx().filesDir, stagingLeaf, destinationLeaf",
            import_flow,
        )
        self.assertIn("godot_napi.cleanupProjectImport(", import_flow)
        self.assertIn("this.getCtx().filesDir, stagingLeaf", import_flow)
        self.assertNotIn("fileIo.renameSync", import_flow)
        self.assertNotIn("removeImportedTree", self.selector)
        self.assertNotIn("fileIo.rmdirSync", import_flow)
        self.assertNotIn("createAndOpenAt", self.selector)
        self.assertNotIn("Try writing directly", self.selector)

        absolute_open = extract_block(
            self.bridge,
            "static int open_absolute_directory_nofollow",
        )
        self.assertIn('open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC)', absolute_open)
        self.assertIn("openat(", absolute_open)
        self.assertIn("O_NOFOLLOW", absolute_open)

        commit = extract_block(self.bridge, "static int commit_project_import")
        self.assertIn("is_expected_import_staging_leaf", commit)
        for token in (
            "open_projects_directory",
            "fstatat(",
            "AT_SYMLINK_NOFOLLOW",
            "openat(",
            "O_NOFOLLOW",
            "renameat2(",
            "RENAME_NOREPLACE",
        ):
            self.assertIn(token, commit)

        staging_guard = extract_block(
            self.bridge,
            "static bool is_safe_import_staging_leaf",
        )
        self.assertIn('const std::string marker = ".importing"', staging_guard)
        self.assertIn("name[index] != '_'", staging_guard)

        cleanup = extract_block(self.bridge, "static int cleanup_project_import")
        self.assertIn("is_safe_import_staging_leaf", cleanup)

        remove_tree = extract_block(self.bridge, "static int remove_import_tree_at")
        for token in (
            "fstatat(",
            "AT_SYMLINK_NOFOLLOW",
            "openat(",
            "O_NOFOLLOW",
            "fdopendir(",
            "unlinkat(",
        ):
            self.assertIn(token, remove_tree)

    def test_setup_failure_cannot_reach_start(self) -> None:
        init = extract_block(self.main_cpp, "HARMONYOS_EXPORT_FN int harmonyos_godot_init")
        failure = extract_block(init, "if (err != OK)")
        self.assertIn("g_engine_cleanup_done.store(true", failure)
        self.assertIn("return (int)err;", failure)
        self.assertNotIn("continuing", failure)

    def test_cleanup_joins_off_ui_thread_with_state_machine(self) -> None:
        for state in ("STOPPED", "STARTING", "RUNNING", "STOPPING", "JOINING"):
            self.assertIn(state, self.bridge)
        cleanup = extract_block(
            self.bridge,
            "static napi_value NAPI_Cleanup(napi_env env",
        )
        self.assertIn("pthread_create(&cleanup_thread", cleanup)
        self.assertIn("pthread_detach(cleanup_thread)", cleanup)
        self.assertIn("g_stop_requested.store(true", cleanup)
        load_library = extract_block(self.bridge, "static napi_value NAPI_LoadLibrary")
        self.assertNotIn("g_engine_worker_state = EngineWorkerState::RUNNING", load_library)
        worker = extract_block(self.bridge, "static void *init_worker")
        self.assertIn("g_engine_worker_state = EngineWorkerState::RUNNING", worker)
        self.assertIn("g_stop_requested.load", worker)
        cleanup_vm = extract_block(self.vm, "cleanupEngine(): void")
        self.assertIn("let cleanupResult: number = godot_napi.cleanup()", cleanup_vm)
        self.assertIn("if (cleanupResult === 0)", cleanup_vm)
        join = extract_block(self.bridge, "static int join_engine_worker")
        self.assertLess(join.index("pthread_join"), join.index("g_engine_thread_joinable = false"))
        self.assertLess(join.index("pthread_join"), join.index("EngineWorkerState::STOPPED"))
        stopped_pos = join.index("g_engine_worker_state = EngineWorkerState::STOPPED")
        self.assertLess(join.index("g_stop_requested.store(false"), stopped_pos)
        self.assertLess(join.index("g_init_status.store(-1"), stopped_pos)
        engine_cleanup = extract_block(
            self.main_cpp,
            "HARMONYOS_EXPORT_FN void harmonyos_godot_cleanup",
        )
        self.assertIn("g_engine_stopped.store(true", engine_cleanup)
        self.assertNotIn("while (", engine_cleanup)
        self.assertNotIn("pthread_join", engine_cleanup)
        self.assertNotIn("dlclose(", self.bridge)

    def test_xcomponent_refresh_and_destroy_tombstone_are_explicit(self) -> None:
        capture = extract_block(
            self.bridge,
            "static bool capture_xcomponent_from_value(napi_env env, napi_value value, const char *source) {",
        )
        self.assertNotIn("if (g_xcomponent != nullptr)", capture)
        self.assertIn("OH_NATIVE_XCOMPONENT_OBJ", capture)
        self.assertIn("g_xcomponent = xcomponent", capture)
        init_xcomponent = extract_block(self.bridge, "static napi_value NAPI_InitXComponent")
        self.assertIn("argc < 1", init_xcomponent)
        self.assertIn(
            'capture_xcomponent_from_value(env, args[0], "onLoad context")',
            init_xcomponent,
        )
        self.assertIn("setter(xcomponent)", init_xcomponent)
        load_library = extract_block(self.bridge, "static napi_value NAPI_LoadLibrary")
        self.assertNotIn("!capture_xcomponent(env) ||", load_library)
        self.assertIn("OH_NativeXComponent *xcomponent = get_current_xcomponent()", load_library)
        self.assertRegex(self.surface, r"\.onLoad\(\(context\?: object\) =>")
        self.assertIn("godot_napi.initXComponent(context)", self.surface)
        self.assertIn("if (!this.nativeXComponentReady)", self.surface)
        self.assertIn('dlsym(g_libgodot, "harmonyos_godot_set_xcomponent")', self.bridge)
        self.assertIn("SurfaceRequestType::UPDATE_XCOMPONENT", self.main_cpp)
        set_xcomponent = extract_block(
            self.main_cpp,
            "HARMONYOS_EXPORT_FN int harmonyos_godot_set_xcomponent",
        )
        self.assertIn("_queue_surface_request(request)", set_xcomponent)

        napi_destroy = extract_block(self.bridge, "static napi_value NAPI_OnSurfaceDestroy")
        self.assertLess(
            napi_destroy.index("record_destroyed_surface_generation"),
            napi_destroy.index("surface_destroy ?"),
        )
        self.assertIn("g_last_destroyed_surface_generation", self.bridge)
        worker = extract_block(self.bridge, "static void *init_worker")
        self.assertIn("g_set_destroyed_surface_generation_func(", worker)
        engine_destroy = extract_block(
            self.main_cpp,
            "HARMONYOS_EXPORT_FN int harmonyos_godot_surface_destroy",
        )
        self.assertLess(
            engine_destroy.index("_record_destroyed_surface_generation"),
            engine_destroy.index("!g_engine_initialized.load"),
        )

    def test_diraccess_mutations_use_dirfd_without_following_links(self) -> None:
        sandbox_root = extract_block(self.dir_cpp, "static int _open_sandbox_root")
        self.assertIn("open(root_utf8.get_data()", sandbox_root)
        self.assertIn("O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW", sandbox_root)
        self.assertIn("fstat(root_fd", sandbox_root)
        self.assertNotIn('open("/"', sandbox_root)
        self.assertIn("O_NOFOLLOW", sandbox_root)
        self.assertNotIn("_path_contains_symlink_or_error", sandbox_root)
        rename = extract_block(self.dir_cpp, "Error DirAccessHarmonyOS::rename")
        remove = extract_block(self.dir_cpp, "Error DirAccessHarmonyOS::remove")
        for token in ("_open_sandbox_parent", "fstatat(", "AT_SYMLINK_NOFOLLOW", "renameat("):
            self.assertIn(token, rename)
        for token in ("_open_sandbox_parent", "fstatat(", "AT_SYMLINK_NOFOLLOW", "unlinkat("):
            self.assertIn(token, remove)
        self.assertNotIn("DirAccessUnix::rename", rename)
        self.assertNotIn("DirAccessUnix::remove", remove)

    def test_surface_lifecycle_runs_on_engine_thread(self) -> None:
        release = extract_block(
            self.display_cpp,
            "void DisplayServerHarmonyOS::release_rendering_window",
        )
        self.assertLess(
            release.index("rendering_device->screen_free"),
            release.index("ctx->window_destroy"),
        )

        apply_destroy = extract_block(
            self.main_cpp,
            "static bool _apply_surface_destroyed_on_engine_thread",
        )
        self.assertLess(
            apply_destroy.index("release_rendering_window"),
            apply_destroy.index("g_native_window->destroy()"),
        )

        surface_create = extract_block(
            self.main_cpp,
            "HARMONYOS_EXPORT_FN int harmonyos_godot_surface_created",
        )
        surface_destroy = extract_block(
            self.main_cpp,
            "HARMONYOS_EXPORT_FN int harmonyos_godot_surface_destroy",
        )
        self.assertIn("_queue_surface_request(request)", surface_create)
        self.assertIn("_queue_surface_request(request)", surface_destroy)
        self.assertNotIn("initialize_with_surface_id", surface_create)
        self.assertNotIn("release_rendering_window", surface_destroy)
        self.assertNotIn("g_native_window->destroy()", surface_destroy)

        frame_loop = self.main_cpp[self.main_cpp.index("while (!g_engine_stopped"):]
        self.assertLess(
            frame_loop.index("_process_surface_requests_on_engine_thread()"),
            frame_loop.index("if (g_paused.load"),
        )

        self.assertEqual(self.surface.count("godot_napi.onSurfaceDestroy("), 1)
        self.assertIn(
            "godot_napi.onSurfaceDestroy(this.reportedSurfaceId, this.surfaceGeneration)",
            self.surface,
        )
        self.assertNotIn("aboutToDisappear(): void", self.surface)
        surface_ready = extract_block(self.page, "onSurfaceReady =")
        recreate = extract_block(surface_ready, "if (this.engineInitRequested)")
        self.assertIn("this.viewModel.onSurfaceCreated(surfaceId, width, height, generation)", recreate)
        self.assertNotIn("this.viewModel.isEngineReady", recreate)
        self.assertIn("let nextSurfaceGeneration: number = 1", self.surface)
        self.assertNotIn("Date.now()", self.surface)
        report_ready = extract_block(self.surface, "private reportSurfaceReady")
        self.assertIn("if (this.surfaceDestroyReported)", report_ready)
        self.assertIn("g_last_destroyed_surface_generation", self.main_cpp)
        apply_create = extract_block(
            self.main_cpp,
            "static int _apply_surface_created_on_engine_thread",
        )
        self.assertIn("p_generation <= destroyed_generation", apply_create)
        self.assertIn("_record_destroyed_surface_generation(p_generation)", apply_destroy)

    def test_title_callback_uses_threadsafe_function(self) -> None:
        self.assertIn("napi_create_threadsafe_function", self.bridge)
        self.assertIn("napi_release_threadsafe_function", self.bridge)
        notify = extract_block(
            self.bridge,
            'extern "C" void harmonyos_notify_window_title(const char *title) {',
        )
        self.assertIn("napi_call_threadsafe_function", notify)
        self.assertNotIn("napi_call_function", notify)
        self.assertNotIn("napi_env", notify)
        self.assertNotIn("g_title_callback_env", self.bridge)
        self.assertNotIn("g_title_callback_ref", self.bridge)
        self.assertIn("harmonyos_set_window_title_callback", self.display_cpp)
        self.assertRegex(
            self.bridge,
            r'dlsym\(\s*g_libgodot,\s*"harmonyos_set_window_title_callback"\)',
        )
        self.assertIn(
            "g_set_window_title_callback_func(harmonyos_notify_window_title)",
            self.bridge,
        )
        self.assertNotIn("__attribute__((weak))", self.display_cpp)

    def test_packaging_builds_both_abis_by_default(self) -> None:
        self.assertIn('ARCH="all"', self.build_script)
        self.assertNotIn("DEVICE_ARCH=", self.build_script)
        self.assertIn(
            'build_project.sh" --debug --arch all',
            self.smoke_script,
        )

    def test_packaged_hap_matches_current_port_sources(self) -> None:
        hap = (
            DEVECO_ROOT
            / "entry/build/default/outputs/default/entry-default-unsigned.hap"
        )
        self.assertTrue(hap.is_file(), "current unsigned HAP is missing")

        excluded_parts = {
            "build",
            "libs",
            "node_modules",
            "oh_modules",
            ".hvigor",
            ".signing",
            ".cxx",
        }
        source_suffixes = {
            ".cpp",
            ".h",
            ".ets",
            ".ts",
            ".json",
            ".json5",
            ".cmake",
            ".sh",
            ".bat",
        }
        source_names = {"CMakeLists.txt", "SConstruct", "SCsub", "detect.py"}
        current_sources = []
        for path in HARMONYOS_ROOT.rglob("*"):
            if not path.is_file():
                continue
            if path.name == "signing.local.json":
                continue
            relative = path.relative_to(HARMONYOS_ROOT)
            if any(part in excluded_parts for part in relative.parts):
                continue
            if path.suffix in source_suffixes or path.name in source_names:
                current_sources.append(path)
        self.assertTrue(current_sources, "no HarmonyOS port sources found")

        newest_source = max(current_sources, key=lambda path: path.stat().st_mtime_ns)
        self.assertGreaterEqual(
            hap.stat().st_mtime_ns,
            newest_source.stat().st_mtime_ns,
            f"HAP is stale; newest source is {newest_source}",
        )

        with zipfile.ZipFile(hap) as package:
            entries = set(package.namelist())
        for abi in ("x86_64", "arm64-v8a"):
            self.assertIn(f"libs/{abi}/libgodot.so", entries)
            self.assertIn(f"libs/{abi}/libgodot_napi.so", entries)

    def test_mouse_coordinates_and_button_mapping(self) -> None:
        self.assertIn("uiContext.vp2px(event.x ?? 0)", self.surface)
        self.assertIn("uiContext.vp2px(event.y ?? 0)", self.surface)
        self.assertIn("uiContext.vp2px(t.x)", self.surface)
        self.assertIn("uiContext.vp2px(t.y)", self.surface)
        self.assertNotRegex(self.surface, r"(?<!\.)\bvp2px\(")

        mapping = extract_block(self.surface, "private mapMouseButton")
        expected = {
            "Left": 0,
            "Middle": 1,
            "Right": 2,
            "Back": 3,
            "Forward": 4,
        }
        for name, value in expected.items():
            self.assertRegex(
                mapping,
                rf"case MouseButton\.{name}:\s*return {value};",
            )

    def test_ime_allocates_one_event_per_character(self) -> None:
        body = extract_block(self.input_cpp, "void process_input_text")
        loop_pos = body.index("for (int i = 0; i < decoded.length(); i++)")
        instantiate_pos = body.index("key_event.instantiate()")
        self.assertGreater(instantiate_pos, loop_pos)
        self.assertEqual(body.count("key_event.instantiate()"), 1)
        self.assertLess(instantiate_pos, body.index("parse_input_event(key_event)"))

    def test_types_match_native_exports(self) -> None:
        for export in (
            "commitProjectImport",
            "cleanupProjectImport",
            "loadLibrary",
            "isLibraryLoaded",
            "startEngine",
            "cleanup",
            "onSurfaceCreated",
            "initXComponent",
            "onSurfaceDestroy",
            "sendKeyEvent",
            "sendMouseEvent",
            "sendTouchEvent",
            "sendInputText",
            "onPause",
            "onResume",
            "onBackPress",
            "registerTitleCallback",
        ):
            self.assertRegex(self.types, rf"\b{export}\s*:")
        self.assertRegex(
            self.types,
            r"initXComponent\s*:\s*\(context:\s*object\)\s*=>\s*number",
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
