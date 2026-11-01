#!/usr/bin/env python3
"""Build a self-contained configurator page from the maintained web sources."""
import argparse
from pathlib import Path


def bundle(source: Path) -> str:
    html = (source / "index.html").read_text(encoding="utf-8")
    css = (source / "app.css").read_text(encoding="utf-8")
    script = (source / "app.js").read_text(encoding="utf-8")
    style_tag = '<link rel="stylesheet" href="/app.css">'
    script_tag = '<script src="/app.js" defer></script>'
    if html.count(style_tag) != 1 or html.count(script_tag) != 1:
        raise ValueError("Expected exactly one stylesheet and one script reference")
    if "</style" in css.lower() or "</script" in script.lower():
        raise ValueError("Web assets contain an inline closing tag")
    html = html.replace(style_tag, f"<style>\n{css}</style>")
    html = html.replace(script_tag, "")
    # Inline scripts do not honor defer; run only after the DOM exists.
    return html.replace("</body>", f"<script>\n{script}</script>\n</body>")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(bundle(args.source), encoding="utf-8")


if __name__ == "__main__":
    main()
