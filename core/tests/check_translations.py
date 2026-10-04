#!/usr/bin/env python3
"""Key parity check for the translation files (also usable by hand: python3 core/tests/check_translations.py).

Compares every resources/lang/*.txt with en.txt: same keys, same {placeholders} per key, no empty values,
valid UTF-8, no leftovers of \\x escapes or printf formats. Exit code 1 if anything is wrong.
"""
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
LANG_DIR = os.path.join(HERE, "..", "resources", "lang")


def load(path):
    raw = open(path, "rb").read()
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError as e:
        raise SystemExit(f"{path}: not valid UTF-8 ({e})")
    if text.startswith("\ufeff"):
        text = text[1:]
    d = {}
    for n, line in enumerate(text.splitlines(), 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            raise SystemExit(f"{path}:{n}: line without '='")
        k, v = line.split("=", 1)
        d[k.strip()] = v.strip()
    return d


def main():
    en = load(os.path.join(LANG_DIR, "en.txt"))
    problems = 0
    for path in sorted(glob.glob(os.path.join(LANG_DIR, "*.txt"))):
        lang = os.path.basename(path)[:-4]
        d = load(path)
        for k in sorted(set(en) - set(d)):
            print(f"{lang}: missing key {k}"); problems += 1
        for k in sorted(set(d) - set(en)):
            print(f"{lang}: extra key {k}"); problems += 1
        for k, v in d.items():
            if not v:
                print(f"{lang}: empty value {k}"); problems += 1
            if k in en and sorted(set(re.findall(r"\{(\w+)\}", v))) != sorted(set(re.findall(r"\{(\w+)\}", en[k]))):
                print(f"{lang}: placeholders differ in {k}"); problems += 1
            if "\\x" in v or "%d" in v or "%%" in v or "\\n" in v:
                print(f"{lang}: leftover escape or printf format in {k}"); problems += 1
            if v.count("{") != v.count("}"):
                print(f"{lang}: unbalanced braces in {k}"); problems += 1
        print(f"{lang}: {len(d)} keys")
    print("OK" if problems == 0 else f"{problems} problem(s)")
    return 0 if problems == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
