import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WEB = ROOT / "src" / "settings" / "web"


class SettingsDesignAssetsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.index = (WEB / "index.html").read_text(encoding="utf-8")
        cls.css = (WEB / "style.css").read_text(encoding="utf-8")
        cls.js = (WEB / "app.js").read_text(encoding="utf-8")

    def test_production_ui_stays_framework_free(self):
        combined = "\n".join([self.index, self.css, self.js]).lower()
        self.assertNotIn("react", combined)
        self.assertNotIn("babel", combined)
        self.assertNotIn("tailwind", combined)

    def test_design_tokens_define_autowhisper_foundation(self):
        for token in [
            "--aw-paper-0",
            "--aw-ink-0",
            "--aw-signal",
            "--aw-ok",
            "--aw-warn",
            "--aw-err",
            "--aw-focus-ring",
            "--aw-font-sans",
        ]:
            self.assertIn(token + ":", self.css)
        self.assertNotRegex(self.css, r"#[fF]{3,6}\b|#[0]{3,6}\b")

    def test_settings_shell_has_intent_navigation_and_live_status(self):
        self.assertIn('class="aw-shell"', self.index)
        self.assertIn('id="settings-nav"', self.index)
        self.assertIn('aria-live="polite"', self.index)
        for label in [
            "Dictation behavior",
            "Model & performance",
            "Audio input",
            "Output & insertion",
            "Privacy",
            "Feedback & tray",
            "Diagnostics",
            "Advanced",
        ]:
            self.assertIn(label, self.index)

    def test_script_preserves_schema_rendering_and_tracks_unsaved_changes(self):
        self.assertIn("/api/schema", self.js)
        self.assertIn("/api/config", self.js)
        self.assertIn("/api/defaults", self.js)
        self.assertIn("/api/platform", self.js)
        self.assertIn("const IA_SECTIONS", self.js)
        self.assertIn("dirtyKeys", self.js)
        self.assertIn("data-config-key", self.js)
        self.assertIn("syncMatchingInputs", self.js)
        self.assertRegex(self.js, r"addEventListener\(\s*[\"']input[\"']")
        self.assertRegex(self.js, r"addEventListener\(\s*[\"']change[\"']")

    def test_script_avoids_duplicate_ids_for_advanced_controls(self):
        self.assertIn("inputId(paneId, section, keyDef.key)", self.js)
        self.assertIn('id + "-desc"', self.js)
        self.assertIn('id + "-issue"', self.js)
        self.assertIn("document.querySelectorAll(`[data-config-key=\"${section}.${keyDef.key}\"]`)", self.js)

    def test_checkbox_descriptions_attach_to_form_controls(self):
        self.assertIn('controlWrap.querySelector("input, select")', self.js)
        self.assertNotIn('controlWrap.firstElementChild?.setAttribute?.("aria-describedby"', self.js)

    def test_active_pane_is_deep_linkable_and_accessible(self):
        self.assertIn("window.location.hash", self.js)
        self.assertIn('window.addEventListener("hashchange"', self.js)
        self.assertIn('aria-current', self.js)
        self.assertIn('history.replaceState', self.js)

    def test_nav_targets_match_ia_sections(self):
        section_ids = set(re.findall(r'id: "([a-z-]+)"', self.js))
        nav_targets = set(re.findall(r'data-target="([a-z-]+)"', self.index))
        self.assertEqual(nav_targets, section_ids)

    def test_save_errors_surface_structured_field_issues(self):
        for snippet in [
            "applyIssues",
            "clearIssues",
            "body?.issues",
            "aria-invalid",
            "data-config-key",
            "aw-issue",
            "has-error",
            "has-warning",
        ]:
            self.assertIn(snippet, self.js)

    def test_field_issue_styles_distinguish_errors_and_warnings(self):
        for snippet in [
            ".aw-field.has-error",
            ".aw-field.has-warning",
            ".aw-issue",
            "var(--aw-err)",
            "var(--aw-warn)",
        ]:
            self.assertIn(snippet, self.css)

    def test_platform_diagnostics_surface_desktop_port_readiness(self):
        for snippet in [
            "platformDiagnostics",
            "renderPlatformDiagnostics",
            "aw-platform-grid",
            "aw-platform-feature",
            "placeholder",
            "unsupported",
        ]:
            self.assertIn(snippet, self.js + self.css)


if __name__ == "__main__":
    unittest.main()
