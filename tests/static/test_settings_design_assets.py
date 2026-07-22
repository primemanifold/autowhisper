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
            "Ask Fabric",
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

    def test_ask_fabric_is_explicit_and_action_safe(self):
        self.assertIn("Ask Fabric is disabled by default", self.js)
        self.assertIn("non-terminal safe toolset", self.js)
        self.assertIn("Dictate always stays", self.js)

    def test_mobile_action_row_cannot_widen_the_settings_viewport(self):
        self.assertRegex(
            self.css,
            r"\.aw-actions\s*\{[^}]*width:\s*100%;[^}]*min-width:\s*0;",
        )
        self.assertIn(".aw-main { width: 100%; max-width: 100vw; }", self.css)

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

    def test_settings_ui_surfaces_the_app_version(self):
        # The user could not tell which version they were running; the UI now
        # shows it (from /api/platform) in the sidebar and the window title.
        self.assertIn("showVersion", self.js)
        self.assertIn("platformDiagnostics?.version", self.js)
        self.assertIn("aw-brand-note", self.js)
        self.assertIn('document.title', self.js)

    def test_model_availability_card_uses_single_readiness_truth(self):
        # The model card must derive both the headline and the row badges from
        # the one `downloaded` flag in /api/models — never two independent
        # states (the macOS "download recommended" + "model ready" bug).
        self.assertIn("/api/models", self.js)
        self.assertIn("renderModelCard", self.js)
        self.assertIn("m.downloaded", self.js)
        self.assertIn("aw-model-card", self.js + self.css)
        self.assertIn(".aw-model-headline.ready", self.css)
        self.assertIn(".aw-model-headline.absent", self.css)
        self.assertIn("aw-model-badge", self.css)

    def test_hotkey_capture_widget_records_chords(self):
        # The trigger field keeps its editable input (collect/setInput stay
        # the same) and adds a Record button that captures a real key chord.
        self.assertIn("renderHotkeyCapture", self.js)
        self.assertIn('name === "hotkeys.trigger"', self.js)
        self.assertIn('name === "hotkeys.ask_trigger"', self.js)
        self.assertIn("aw-hotkey-record", self.js + self.css)
        self.assertIn('"metaKey", "super"', self.js)  # Command -> super token

    def test_ask_fabric_is_explicit_opt_in_with_separate_mode_copy(self):
        for snippet in [
            'id: "fabric"',
            "disabled by default",
            "Dictate always stays",
            "private stdin pipe",
        ]:
            self.assertIn(snippet, self.js)

    def test_permissions_pane_surfaces_os_grants(self):
        # macOS TCC status with a deep link per denied grant — the fix for the
        # silently-blocked push-to-talk.
        self.assertIn("/api/permissions", self.js)
        self.assertIn("renderPermissions", self.js)
        self.assertIn("deep_link", self.js)
        self.assertIn("aw-perm-row", self.css)
        self.assertIn(".aw-perm-row.err .aw-perm-dot", self.css)

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
