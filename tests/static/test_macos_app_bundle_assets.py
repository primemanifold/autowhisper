import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUNDLE_CMAKE = ROOT / "cmake" / "MacOSBundle.cmake"
INFO_PLIST = ROOT / "platform" / "macos" / "Info.plist.in"
ENTITLEMENTS = ROOT / "platform" / "macos" / "entitlements.plist"
INSTALL = ROOT / "platform" / "macos" / "install.sh"
SMOKE = ROOT / "scripts" / "macos_app_smoke.sh"
MAIN_CPP = ROOT / "src" / "main.cpp"
CONFIG_CPP = ROOT / "src" / "config" / "config.cpp"
TRAY_MACOS = ROOT / "src" / "tray" / "platform" / "tray_macos.mm"
DAEMON_CPP = ROOT / "src" / "daemon" / "daemon.cpp"
DAEMON_MACOS = ROOT / "src" / "daemon" / "daemon_macos.mm"
HOTKEY_MACOS = ROOT / "src" / "hotkey" / "platform" / "hotkey_macos.mm"
SWIFT_SETTINGS = ROOT / "platform" / "macos" / "SettingsApp.swift"


class MacOSAppBundleAssetsTest(unittest.TestCase):
    def test_bundle_target_is_part_of_default_macos_build(self):
        cmake = BUNDLE_CMAKE.read_text(encoding="utf-8")
        self.assertRegex(
            cmake,
            r"add_custom_target\(\s*autowhisper_bundle\s+ALL\b",
            "macOS builds should emit AutoWhisper.app by default, not hide it behind an opt-in target",
        )
        self.assertIn("DEPENDS autowhisper", cmake)
        self.assertIn('"${CMAKE_SOURCE_DIR}/config.toml" "${AUTOWHISPER_RESOURCES_DIR}/config.toml"', cmake)
        self.assertIn("codesign --force", cmake)
        self.assertIn("codesign --verify --deep --strict", cmake)
        self.assertNotIn("codesign failed (ignored for dev)", cmake)
        self.assertIn("--entitlements", cmake)
        self.assertIn("--timestamp", cmake)
        self.assertNotIn("--timestamp=none", cmake)
        self.assertIn("AutoWhisper.app built at", cmake)

    def test_info_plist_declares_menu_bar_identity_and_permissions(self):
        plist = INFO_PLIST.read_text(encoding="utf-8")
        for snippet in [
            "CFBundleIdentifier",
            "us.primemanifold.autowhisper",
            "CFBundlePackageType",
            "APPL",
            "CFBundleExecutable",
            "autowhisper",
            "LSUIElement",
            "NSMicrophoneUsageDescription",
            "NSAppleEventsUsageDescription",
        ]:
            self.assertIn(snippet, plist)
        self.assertRegex(plist, r"Audio never leaves this (Mac|device)")
        self.assertNotIn("TODO", plist)

    def test_app_bundle_double_click_runs_the_foreground_daemon(self):
        main = MAIN_CPP.read_text(encoding="utf-8")
        self.assertIn("launched_from_macos_app_bundle", main)
        self.assertIn("has_only_launchservices_args", main)
        self.assertIn('"-psn_"', main)
        self.assertIn('"run"', main)
        self.assertRegex(
            main,
            r"launched_from_macos_app_bundle[\s\S]+has_only_launchservices_args[\s\S]+app_launch_args",
            "Double-clicking AutoWhisper.app must not execute the CLI with no subcommand and exit after printing help",
        )
        self.assertIn("app_launch_argv.push_back(nullptr)", main)

    def test_app_bundle_double_click_can_find_bundled_default_config(self):
        config_cpp = CONFIG_CPP.read_text(encoding="utf-8")
        self.assertIn("find_bundled_config_file", config_cpp)
        self.assertIn('"Resources" / "config.toml"', config_cpp)
        self.assertRegex(
            config_cpp,
            r"get_user_config_path\(\),[\s\S]+bundled_config",
            "A double-clicked app has no repo cwd, so config lookup must fall back to the bundled default after user config",
        )

    def test_open_settings_opens_web_ui_hybrid(self):
        # Hybrid (2026-06-14): the tray "Open Settings" opens the cross-platform
        # web settings UI by re-execing `config ui` — so macOS gets the themes,
        # the cast, working navigation, the model availability card, the hotkey
        # capture widget, and the permissions pane, instead of the limited
        # native helper. The SwiftUI helper stays only for first-run/onboarding
        # (still built and codesigned in the bundle).
        cmake = BUNDLE_CMAKE.read_text(encoding="utf-8")
        tray = TRAY_MACOS.read_text(encoding="utf-8")
        self.assertIn("onOpenSettings", tray)
        self.assertIn('@"config", @"ui"', tray, "macOS Open Settings should launch the web settings UI via `config ui`")
        self.assertIn('@"--config"', tray)
        # The native first-run/onboarding helper is still part of the bundle.
        self.assertTrue(SWIFT_SETTINGS.exists())
        self.assertIn("AUTOWHISPER_SETTINGS_HELPER", cmake)
        self.assertIn("swiftc", cmake)
        self.assertIn("AutoWhisperSettings", cmake)
        self.assertRegex(cmake, r"codesign --force[\s\S]+AutoWhisperSettings")

    def test_macos_bundle_has_an_app_icon(self):
        # The app must have a real Dock/Finder/window icon (CFBundleIconFile),
        # not the generic blank one. The committed .iconset is assembled into
        # AppIcon.icns by iconutil at build time.
        iconset = ROOT / "platform" / "macos" / "AutoWhisper.iconset"
        self.assertTrue(iconset.is_dir(), "missing AutoWhisper.iconset")
        for name in [
            "icon_16x16.png", "icon_16x16@2x.png", "icon_32x32.png", "icon_32x32@2x.png",
            "icon_128x128.png", "icon_128x128@2x.png", "icon_256x256.png",
            "icon_256x256@2x.png", "icon_512x512.png", "icon_512x512@2x.png",
        ]:
            f = iconset / name
            self.assertTrue(f.exists(), f"iconset missing {name}")
            self.assertEqual(f.read_bytes()[:8], b"\x89PNG\r\n\x1a\n", f"{name} is not a PNG")
        plist = INFO_PLIST.read_text(encoding="utf-8")
        self.assertIn("CFBundleIconFile", plist)
        self.assertIn("AppIcon.icns", plist)
        cmake = BUNDLE_CMAKE.read_text(encoding="utf-8")
        self.assertIn("iconutil", cmake)
        self.assertIn("AutoWhisper.iconset", cmake)
        self.assertIn("AppIcon.icns", cmake)

    def test_macos_settings_open_in_a_native_webview_window(self):
        # The macOS settings UI is the web UI hosted in a real, front-most
        # WKWebView app window (NSWindow), not a browser tab that gets buried.
        # cmd_config_ui launches the bundled helper with --url; the helper
        # opens the window and brings it to front.
        swift = SWIFT_SETTINGS.read_text(encoding="utf-8")
        self.assertIn("import WebKit", swift)
        self.assertIn("WKWebView", swift)
        self.assertIn('firstIndex(of: "--url")', swift)
        self.assertIn("makeKeyAndOrderFront", swift)
        self.assertIn("NSApp.activate(ignoringOtherApps: true)", swift)

        cli = (ROOT / "src" / "cli" / "cli_settings_ui.cpp").read_text(encoding="utf-8")
        self.assertIn("settings_helper_path", cli)
        self.assertIn("AutoWhisperSettings", cli)
        self.assertIn('"--url"', cli)

        cmake = BUNDLE_CMAKE.read_text(encoding="utf-8")
        self.assertIn("-framework WebKit", cmake)

        # WKWebView must be allowed to load the local http settings server.
        plist = INFO_PLIST.read_text(encoding="utf-8")
        self.assertIn("NSAppTransportSecurity", plist)
        self.assertIn("NSAllowsLocalNetworking", plist)

    def test_app_bundle_launch_opens_setup_instead_of_silent_exit_when_not_ready(self):
        main = MAIN_CPP.read_text(encoding="utf-8")
        config_cpp = CONFIG_CPP.read_text(encoding="utf-8")
        self.assertIn("ensure_user_config_file", config_cpp)
        self.assertRegex(
            main,
            r"launched_from_macos_app_bundle[\s\S]+\"run\"",
            "Double-click app launch should still route through foreground run",
        )
        cli = (ROOT / "src" / "cli" / "cli.cpp").read_text(encoding="utf-8")
        self.assertIn("aw_macos_is_app_bundle_launch", cli)
        self.assertIn("aw_macos_launch_setup_helper", cli)
        self.assertIn("aw_macos_prompt_required_permissions", cli)
        self.assertRegex(
            cli,
            r"catch \(const std::exception& e\)[\s\S]+aw_macos_launch_setup_helper",
            "A first-run model/config/permission failure from the app bundle should open setup UI instead of disappearing",
        )

    def test_native_settings_contains_first_run_onboarding_actions(self):
        swift = SWIFT_SETTINGS.read_text(encoding="utf-8")
        for snippet in [
            "First-run setup",
            "Download recommended model",
            "Input Monitoring",
            "Accessibility",
            "Microphone",
            "Privacy_ListenEvent",
            "Privacy_Accessibility",
            "Privacy_Microphone",
            "--setup-error",
            "model", "download",
        ]:
            self.assertIn(snippet, swift)
        self.assertIn("Process()", swift)
        self.assertIn("readabilityHandler", swift)
        self.assertIn("waitUntilExit", swift)
        self.assertIn("AVCaptureDevice.requestAccess", swift)

    def test_macos_signal_sources_do_not_capture_stack_shutdown_pointer(self):
        daemon_cpp = DAEMON_CPP.read_text(encoding="utf-8")
        daemon_macos = DAEMON_MACOS.read_text(encoding="utf-8")
        self.assertIn("aw_macos_teardown_signals", daemon_cpp)
        self.assertIn("aw_macos_teardown_signals", daemon_macos)
        self.assertIn("g_shutdown_flag", daemon_macos)
        self.assertIn("g_shutdown_flag = shutdown_flag", daemon_macos)
        self.assertIn("g_shutdown_flag = nullptr", daemon_macos)
        self.assertNotIn("if (shutdown_flag) shutdown_flag->store", daemon_macos)

    def test_macos_hotkey_event_tap_is_released_on_listener_thread(self):
        hotkey_macos = HOTKEY_MACOS.read_text(encoding="utf-8")
        self.assertIn("CFRunLoopRemoveSource(impl_->run_loop", hotkey_macos)
        self.assertIn("CFRelease(impl_->src)", hotkey_macos)
        self.assertRegex(
            hotkey_macos,
            r"CFRunLoopRun\(\);[\s\S]+CFRunLoopRemoveSource\(impl_->run_loop[\s\S]+CFRelease\(impl_->src\)",
        )
        self.assertIn("listener thread releases the event tap", hotkey_macos)

    def test_entitlements_are_minimal_and_audio_focused(self):
        entitlements = ENTITLEMENTS.read_text(encoding="utf-8")
        self.assertIn("com.apple.security.device.audio-input", entitlements)
        self.assertNotIn("com.apple.security.cs.disable-library-validation", entitlements)
        self.assertNotIn("com.apple.security.network.client", entitlements)

    def test_install_script_points_at_app_bundle_and_launch_agent(self):
        script = INSTALL.read_text(encoding="utf-8")
        for snippet in [
            "AutoWhisper.app",
            "/Applications/AutoWhisper.app",
            "Contents/MacOS/autowhisper",
            "Library/LaunchAgents/us.primemanifold.autowhisper.plist",
            "autowhisper_bundle",
        ]:
            self.assertIn(snippet, script)

    def test_smoke_script_validates_built_app_bundle(self):
        self.assertTrue(SMOKE.exists(), "scripts/macos_app_smoke.sh should provide repeatable local proof")
        script = SMOKE.read_text(encoding="utf-8")
        for snippet in [
            "AutoWhisper.app/Contents/MacOS/autowhisper",
            "AutoWhisper.app/Contents/Info.plist",
            "plutil -lint",
            "codesign --verify",
            "spctl",
            "--version",
            "CFBundleIdentifier",
        ]:
            self.assertIn(snippet, script)
        self.assertNotRegex(script, r"sudo\s+")


if __name__ == "__main__":
    unittest.main()
