#!/usr/bin/env python3
"""Cask translation check.

Every tr("...") string from src/ must be present in all resources/lang/*.json, with no extra
keys, and with the same placeholders (%1, %2...) in the translation.

    scripts/check-translations.py            check (exit code 1 on errors)
    scripts/check-translations.py --list     print a template for a new language
    scripts/check-translations.py --sync-en  regenerate resources/lang/en.json (reference template)
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
TR_RE = re.compile(r'(?<![A-Za-z0-9_])tr\(\s*((?:"(?:[^"\\]|\\.)*"\s*)+)')
LIT_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')
PH_RE = re.compile(r'%\d+')


def extract():
    """Returns {key: 'file:line'}."""
    keys = {}
    for path in sorted((ROOT / "src").rglob("*")):
        if path.suffix not in (".cpp", ".h"):
            continue
        text = path.read_text(encoding="utf-8")
        for m in TR_RE.finditer(text):
            key = "".join(json.loads('"' + lit + '"') for lit in LIT_RE.findall(m.group(1)))
            line = text.count("\n", 0, m.start()) + 1
            keys.setdefault(key, f"{path.relative_to(ROOT)}:{line}")
    return keys


def main():
    keys = extract()

    en_path = ROOT / "resources" / "lang" / "en.json"
    en_expected = {"_name": "English", **{k: k for k in keys}}

    if "--sync-en" in sys.argv:
        en_path.write_text(json.dumps(en_expected, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"{en_path.relative_to(ROOT)} updated ({len(keys)} strings)")
        return 0

    if "--list" in sys.argv:
        out = {"_name": ""}
        out.update({k: "" for k in keys})
        print(json.dumps(out, ensure_ascii=False, indent=2))
        return 0

    files = sorted((ROOT / "resources" / "lang").glob("*.json"))
    if not files:
        print("no language files in resources/lang")
        return 1

    errors = 0
    for f in files:
        data = json.loads(f.read_text(encoding="utf-8"))
        if f == en_path:
            # The reference template: every string maps to itself.
            if data != en_expected:
                print("en.json is out of date, run: scripts/check-translations.py --sync-en")
                errors += 1
            continue
        if not data.get("_name"):
            print(f"{f.name}: missing _name")
            errors += 1
        for k, where in keys.items():
            v = data.get(k, "")
            if not v:
                print(f"{f.name}: missing translation ({where}): {k!r}")
                errors += 1
            elif sorted(PH_RE.findall(k)) != sorted(PH_RE.findall(v)):
                print(f"{f.name}: placeholders differ: {k!r} -> {v!r}")
                errors += 1
        for k in data:
            if k != "_name" and k not in keys:
                print(f"{f.name}: unused key: {k!r}")
                errors += 1

    print(f"{len(keys)} strings, {len(files)} language file(s), {errors} problem(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
