import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
README = ROOT / "README.md"
MATRIX = ROOT / "engineering" / "platform-readiness-matrix.md"
COMMANDS = ROOT / "engineering" / "platform-validation-commands.md"
MACOS_DOC = ROOT / "docs" / "MACOS.md"
INSTALL_SCRIPT = ROOT / "install.sh"


class PlatformReadinessDocsTests(unittest.TestCase):
    def read(self, path: Path) -> str:
        self.assertTrue(path.exists(), f"Missing expected file: {path}")
        return path.read_text()

    def test_readme_macos_quick_start_mentions_permissions(self):
        text = self.read(README)
        self.assertIn("macOS", text)
        for token in ["Microphone", "Accessibility", "Input Monitoring"]:
            self.assertIn(token, text)
        self.assertIn("AutoWhisper-macOS-v0.7.1.zip", text)

    def test_release_artifact_urls_have_adjacent_versions(self):
        text = self.read(README) + "\n" + self.read(MATRIX) + "\n" + self.read(COMMANDS)
        for match in re.finditer(r"https://github.com/primemanifold/autowhisper/releases/download/[^\s)]+", text):
            window = text[max(0, match.start() - 120): match.end() + 120]
            self.assertIn("v0.7.1", window)

    def test_macos_runtime_e2e_is_not_ready(self):
        text = self.read(MATRIX)
        self.assertRegex(text, r"macOS.*runtime_e2e.*`partial`", "macOS runtime_e2e must remain partial")
        self.assertNotRegex(text, r"macOS.*runtime_e2e.*`ready`", "macOS runtime_e2e cannot be ready yet")

    def test_audio_and_microphone_not_promoted_to_ready_without_evidence(self):
        text = self.read(MATRIX)
        self.assertRegex(text, r"macOS.*microphone.*`partial`")
        self.assertRegex(text, r"Linux.*microphone.*`partial`")
        self.assertIn("does not prove", text.lower())

    def test_no_dmg_claim_without_artifact(self):
        text = self.read(README) + "\n" + self.read(MATRIX) + "\n" + self.read(COMMANDS)
        self.assertNotIn(".dmg", text.lower())
        self.assertIn(".zip", text.lower())

    def test_install_script_is_not_described_as_public_release_installer(self):
        readme = self.read(README)
        if INSTALL_SCRIPT.exists():
            self.assertNotIn("one-command public installer", readme.lower())
        self.assertIn("PPA", readme)

    def test_windows_docs_do_not_overclaim_runtime_support(self):
        text = "\n".join(
            p.read_text() for p in [README, MATRIX, COMMANDS] if p.exists()
        )
        risky = re.compile(r"Windows[^\n]*(ready|supported|works on)", re.IGNORECASE)
        for match in risky.finditer(text):
            line = match.group(0)
            allowed = ["build_proven", "unverified", "not yet", "coming soon", "deferred", "cross-build only", "runtime unproven"]
            self.assertTrue(any(token in line.lower() for token in allowed), line)
    def test_windows_validation_uses_existing_toolchain_file(self):
        text = self.read(COMMANDS)
        self.assertIn("cmake/toolchains/mingw-w64-x86_64.cmake", text)
        self.assertNotIn("cmake/mingw-toolchain.cmake", text)
        toolchain = ROOT / "cmake" / "toolchains" / "mingw-w64-x86_64.cmake"
        self.assertTrue(toolchain.exists(), "Documented Windows cross-build toolchain must exist")

    def test_macos_public_artifact_validation_assesses_extracted_zip(self):
        text = self.read(COMMANDS)
        self.assertIn("unzip -q /tmp/AutoWhisper-macOS-v0.7.1.zip -d /tmp/AutoWhisper-v0.7.1", text)
        self.assertIn("spctl --assess --type execute --verbose /tmp/AutoWhisper-v0.7.1/AutoWhisper.app", text)
        self.assertNotIn("spctl --assess --type execute --verbose /Applications/AutoWhisper.app", text)


if __name__ == "__main__":
    unittest.main()
