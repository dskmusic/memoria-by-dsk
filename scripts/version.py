#!/usr/bin/env python3
"""Prints the project version from CMakeLists.txt (single source of truth)."""
import pathlib, re
root = pathlib.Path(__file__).resolve().parent.parent
print(re.search(r"project\(Memoria VERSION ([0-9.]+)", (root / "CMakeLists.txt").read_text()).group(1))
