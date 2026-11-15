#!/usr/bin/env python3
"""Проверка переводов Cask.

Все строки tr("...") из src/ должны быть во всех resources/lang/*.json, без лишних
ключей, с теми же плейсхолдерами (%1, %2...) в переводе.

    scripts/check-translations.py            проверить (код возврата 1 при ошибках)
    scripts/check-translations.py --list     вывести заготовку для нового языка
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
    """Возвращает {ключ: 'файл:строка'}."""
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
