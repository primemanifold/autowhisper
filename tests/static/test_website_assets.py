import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SITE = ROOT / "site"
PAGES_WORKFLOW = ROOT / ".github" / "workflows" / "pages.yml"


class WebsiteAssetsTest(unittest.TestCase):
    def test_static_site_exists_without_frontend_runtime(self):
        index = SITE / "index.html"
        css = SITE / "styles.css"
        nojekyll = SITE / ".nojekyll"
        self.assertTrue(index.exists(), "GitHub Pages needs a static landing page under site/index.html")
        self.assertTrue(css.exists(), "Landing page styles should live beside the static site")
        self.assertTrue(nojekyll.exists(), "Disable Jekyll so Pages serves plain static assets exactly")
        html = index.read_text(encoding="utf-8").lower()
        css_text = css.read_text(encoding="utf-8").lower()
        combined = html + "\n" + css_text
        for forbidden in ["react", "astro", "vite", "tailwind", "gtag(", "google-analytics", "segment.com"]:
            self.assertNotIn(forbidden, combined)
        self.assertNotRegex(html, r"<script\b", "The marketing site should stay static and script-free for now")

    def test_landing_page_has_release_download_and_install_paths(self):
        html = (SITE / "index.html").read_text(encoding="utf-8")
        for snippet in [
            "AutoWhisper",
            "offline",
            "local-first",
            "Download for macOS",
            "AutoWhisper-macOS-v0.7.0.zip",
            "sudo add-apt-repository ppa:primemanifold/autowhisper",
            "github.com/primemanifold/autowhisper",
            "Notarized Developer ID",
        ]:
            self.assertIn(snippet, html)
        self.assertRegex(
            html,
            r"https://github\.com/primemanifold/autowhisper/releases/(?:latest|download/v0\.7\.0/AutoWhisper-macOS-v0\.7\.0\.zip)",
        )

    def test_landing_page_sets_baseline_metadata_and_accessibility(self):
        html = (SITE / "index.html").read_text(encoding="utf-8")
        self.assertIn('<html lang="en">', html)
        self.assertIn('name="viewport"', html)
        self.assertIn('name="description"', html)
        self.assertIn('href="#main"', html)
        self.assertIn('id="main"', html)
        self.assertRegex(html, r"<h1[\s\S]+</h1>")
        self.assertRegex(html, r"aria-label=\"[^\"]+\"")

    def test_github_pages_workflow_deploys_site_from_core(self):
        self.assertTrue(PAGES_WORKFLOW.exists(), "GitHub Pages deployment workflow is missing")
        workflow = PAGES_WORKFLOW.read_text(encoding="utf-8")
        for snippet in [
            "name: Deploy GitHub Pages",
            "branches: [core]",
            "workflow_dispatch:",
            "contents: read",
            "pages: write",
            "id-token: write",
            "github.event.repository.private == false",
            "actions/configure-pages@v5",
            "enablement: true",
            "actions/upload-pages-artifact@v3",
            "actions/deploy-pages@v4",
            "path: site",
            "environment:",
            "github-pages",
        ]:
            self.assertIn(snippet, workflow)


if __name__ == "__main__":
    unittest.main()
