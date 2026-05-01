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

    def test_open_settings_uses_native_swiftui_helper_not_browser_ui(self):
        cmake = BUNDLE_CMAKE.read_text(encoding="utf-8")
        tray = TRAY_MACOS.read_text(encoding="utf-8")
        self.assertTrue(SWIFT_SETTINGS.exists(), "Open Settings should launch a native SwiftUI helper window, not a browser tab")
        swift = SWIFT_SETTINGS.read_text(encoding="utf-8")
        for snippet in [
            "import SwiftUI",
            "AutoWhisperSettingsApp",
            "SettingsView",
            "ConfigStore",
            "--config",
        ]:
            self.assertIn(snippet, swift)
        self.assertNotIn("WebView", swift)
        self.assertNotIn("WKWebView", swift)
        self.assertIn("AUTOWHISPER_SETTINGS_HELPER", cmake)
        self.assertIn("AUTOWHISPER_SWIFT_TARGET", cmake)
        self.assertIn('-target "${AUTOWHISPER_SWIFT_TARGET}"', cmake)
        self.assertIn("swiftc", cmake)
        self.assertIn("AutoWhisperSettings", cmake)
        self.assertIn('"${AUTOWHISPER_MACOS_DIR}/AutoWhisperSettings"', cmake)
        self.assertIn("codesign --force", cmake)
        self.assertRegex(cmake, r"codesign --force[\s\S]+AutoWhisperSettings")
        self.assertIn("AutoWhisperSettings", tray)
        self.assertIn('@"--config"', tray)
        self.assertNotIn('@"config", @"ui"', tray)
        self.assertNotIn("Grid(", swift)

    def test_native_settings_edits_user_config_not_signed_bundle_resource(self):
        swift = SWIFT_SETTINGS.read_text(encoding="utf-8")
        for snippet in [
            "editableConfigPath",
            "isBundledAppResourceConfig",
            "userConfigPath",
            "createDirectory",
            "copyItem",
            "Contents/Resources/config.toml",
            "ConfigStore(configPath: editableConfigPath)",
        ]:
            self.assertIn(snippet, swift)
        self.assertRegex(
            swift,
            r"isBundledAppResourceConfig[\s\S]+userConfigPath[\s\S]+copyItem",
            "Bundled app Resources/config.toml must be a read-only template copied to the per-user config before editing",
        )
        self.assertRegex(
            swift,
            r"else \{[\s\S]+configPath = ConfigStore\.userConfigPath\(\)",
            "Direct helper launches without --config should use the same HOME-aware user config path as bundled-template launches",
        )
        self.assertRegex(
            swift,
            r"save\(\)[\s\S]+createDirectory\([\s\S]+at:[\s\S]+withIntermediateDirectories: true[\s\S]+write\(",
            "First-run Save should create ~/.config/autowhisper before writing config.toml",
        )
        self.assertRegex(
            swift,
            r"static func defaultConfigText\(\)[\s\S]+\[hotkeys\][\s\S]+\[model\][\s\S]+\[output\][\s\S]+\[audio\][\s\S]+\[feedback\][\s\S]+\[tray\][\s\S]+\[daemon\]",
            "Missing user config must seed a complete TOML template before Save mutates it",
        )
        self.assertRegex(
            swift,
            r"catch \{\s+originalText = Self\.defaultConfigText\(\)\s+draft = Self\.parse\(originalText\)",
            "load() must seed parseable defaults after a missing file so first-run Save cannot create duplicate TOML tables",
        )

    def test_native_settings_default_template_has_no_duplicate_tables(self):
        swift = SWIFT_SETTINGS.read_text(encoding="utf-8")
        match = re.search(r"static func defaultConfigText\(\) -> String \{\s+\"\"\"(?P<body>[\s\S]+?)\"\"\"\s+\}", swift)
        self.assertIsNotNone(match, "Settings helper must keep a concrete default TOML template for first-run Save")
        body = match.group("body")
        tables = re.findall(r"^\s*\[([^\]]+)\]", body, flags=re.MULTILINE)
        self.assertEqual(len(tables), len(set(tables)), f"Default TOML template has duplicate tables: {tables}")
        self.assertEqual(
            {"hotkeys", "model", "output", "audio", "feedback", "tray", "daemon"},
            set(tables),
        )

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
