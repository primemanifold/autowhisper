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
