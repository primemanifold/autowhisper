import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
README = ROOT / "README.md"
IOS_README = ROOT / "ios" / "README.md"
MATRIX = ROOT / "engineering" / "platform-readiness-matrix.md"
COMMANDS = ROOT / "engineering" / "platform-validation-commands.md"
USAGE_GUIDE = ROOT / "docs" / "usage-guide.md"
MEDIA_README = ROOT / "docs" / "media" / "README.md"
MANIM_PLAN = ROOT / "docs" / "media" / "manim" / "autowhisper-flow-plan.md"


class DocsClaimBoundaryTests(unittest.TestCase):
    def read(self, path: Path) -> str:
        self.assertTrue(path.exists(), f"Missing expected file: {path}")
        return path.read_text()

    def all_docs(self) -> str:
        paths = [README, IOS_README, MATRIX, COMMANDS, USAGE_GUIDE, MEDIA_README, MANIM_PLAN]
        return "\n".join(p.read_text() for p in paths if p.exists())

    def test_macos_public_artifact_link_is_v071_zip(self):
        text = self.read(README)
        self.assertIn("v0.7.1", text)
        self.assertIn("AutoWhisper-macOS-v0.7.1.zip", text)
        self.assertNotIn("AutoWhisper-macOS-v0.7.2.zip", text)

    def test_public_site_and_readme_version_language_match_current_release(self):
        text = self.read(README) + "\n" + self.read(MATRIX)
        self.assertIn("v0.7.1", text)
        self.assertNotRegex(text, r"v0\.7\.2[^\n]*(shipped|public|released|notarized)", re.IGNORECASE)

    def test_ios_docs_do_not_claim_real_local_transcription(self):
        text = self.all_docs().lower()
        forbidden = ["ios local whisper works", "ios transcribes locally today", "ios real transcription ready"]
        for phrase in forbidden:
            self.assertNotIn(phrase, text)
        self.assertIn("whisper.cpp bridge pending", text)

    def test_ios_readme_bridge_pending_language_is_preserved(self):
        text = self.read(IOS_README).lower()
        self.assertIn("placeholder", text)
        self.assertTrue("not yet" in text or "pending" in text)
        self.assertIn("widget", text)
        self.assertIn("foreground app", text)

    def test_widget_docs_do_not_claim_direct_recording(self):
        text = self.all_docs().lower()
        self.assertNotIn("widget records audio", text)
        self.assertNotIn("record directly from the widget", text)
        self.assertIn("widget opens", text)
        self.assertIn("foreground app", text)

    def test_windows_docs_keep_runtime_caveat(self):
        text = self.all_docs()
        self.assertIn("Windows", text)
        self.assertIn("build_proven", text)
        self.assertIn("runtime unproven", text)
        self.assertNotRegex(text, r"Windows[^\n]*(ready|fully supported|production ready)", re.IGNORECASE)

    def test_linux_docs_include_ppa_and_wayland_warning(self):
        text = self.read(README) + "\n" + self.read(USAGE_GUIDE)
        self.assertIn("ppa:primemanifold/autowhisper", text)
        self.assertIn("Wayland", text)
        self.assertIn("X11", text)

    def test_no_cross_platform_feature_parity_claim(self):
        text = self.all_docs().lower()
        forbidden = ["feature parity across all platforms", "all platforms are fully supported", "works everywhere"]
        for phrase in forbidden:
            self.assertNotIn(phrase, text)

    def test_local_first_claims_are_backed_by_offline_or_no_cloud_wording(self):
        text = self.all_docs().lower()
        self.assertIn("local-first", text)
        self.assertTrue("offline" in text or "no cloud" in text)
        self.assertNotIn("cloud transcription account required", text)


if __name__ == "__main__":
    unittest.main()
