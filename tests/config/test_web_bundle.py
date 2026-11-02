"""Check the firmware document is self-contained and scripts run after its DOM."""
import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("bundle_configurator", ROOT / "scripts/bundle_configurator.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class WebBundleTests(unittest.TestCase):
    def test_page_contains_complete_assets(self):
        source = ROOT / "src/web"
        html = module.bundle(source)
        self.assertNotIn('href="/app.css"', html)
        self.assertNotIn('src="/app.js"', html)
        self.assertIn((source / "app.css").read_text(), html)
        self.assertIn((source / "app.js").read_text(), html)
        self.assertLess(html.index('id="retry"'), html.index("<script>"))
        self.assertIn('rel="icon" href="data:,"', html)
        self.assertIn('const hidIoRepository = "gennartan/layouts"', html)
        self.assertIn("https://api.github.com/repos/", html)
        self.assertNotIn("bundledLocaleDocuments", html)
        for field in ("to_hid_keyboard", "from_hid_keyboard", "composition"):
            self.assertNotIn(f'"{field}":', html, "Locale JSON must never enter the firmware bundle")

    def test_missing_asset_reference_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            for name in ("index.html", "app.css", "app.js"):
                (source / name).write_text("")
            with self.assertRaises(ValueError):
                module.bundle(source)


if __name__ == "__main__":
    unittest.main()
