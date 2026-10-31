#!/usr/bin/env python3
"""Compare character_spec.hpp constants with the original game's init.js.

Usage: python tests/check_spec.py [path/to/init.js]
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
js_path = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else ROOT / "game" / "js" / "init.js"
hpp_path = ROOT / "src" / "character_spec.hpp"
js = js_path.read_text(encoding="utf-8")
hpp = hpp_path.read_text(encoding="utf-8")

pairs = {
	"jumpImpulse": "jumpImpulse", "jumpVelocity": "jumpVelocity", "jumpIceVelocity": "jumpIceVelocity",
	"runForce": "runForce", "floatForce": "floatForce", "defaultMaxSpeed": "defaultMaxSpeed",
	"waterIceMaxSpeed": "waterIceMaxSpeed", "absoluteMaxSpeed": "absoluteMaxSpeed", "feetFriction": "feetFriction",
	"spriteScale": "spriteScale", "r1": "r1", "r2": "r2", "dr": "dr", "y1": "y1", "y2": "y2",
}
bad = 0
for h, j in pairs.items():
	m = re.search(r"e\.prototype\.%s\s*=\s*(-?[0-9.]+)" % j, js)
	m2 = re.search(r"constexpr double %s\s*=\s*([+-]?[0-9.]+)" % h, hpp)
	if not m or not m2:
		print("MISSING", h)
		bad += 1
		continue
	a, b = float(m.group(1)), float(m2.group(1))
	if h in ("y1", "y2"):
		b = -b
	ok = abs(a - b) < 1e-9
	print(("ok  " if ok else "BAD ") + f"{h}: init.js={a} spec={b}")
	bad += not ok

for pat, label in [
	(r"categoryBits = 16, \w+\.filter\.maskBits = 4353", "char cat/mask 16/4353"),
	(r"h\.density = 1\.4|density = 1\.4", "hull density 1.4"),
	(r"new box2d\.b2CircleShape\(\.1\)", "portal sensor r=0.1"),
	(r"categoryBits = 4096, \w+\.filter\.maskBits = 4096, \w+\.filter\.groupIndex = -1", "portal sensor filter 4096/4096/-1"),
	(r"SetBullet\(!0\)", "bullet body"),
	(r"fixedRotation = !0", "fixed rotation"),
]:
	ok = re.search(pat, js) is not None
	print(("ok  " if ok else "BAD ") + label)
	bad += not ok
print("ALL OK" if not bad else f"{bad} problems")
sys.exit(1 if bad else 0)
