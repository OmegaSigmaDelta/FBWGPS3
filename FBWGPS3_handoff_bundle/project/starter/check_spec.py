#!/usr/bin/env python3
"""Verifies the numbers in src/character_spec.hpp against the original js/init.js. usage: check_spec.py <path-to-init.js>"""
import re, sys
js = open(sys.argv[1] if len(sys.argv) > 1 else "../game/js/init.js", encoding="utf-8").read()
hpp = open(__file__.replace("tests/check_spec.py", "src/character_spec.hpp"), encoding="utf-8").read()
pairs = {  # hpp name -> prototype name in init.js
 "jumpImpulse":"jumpImpulse","jumpVelocity":"jumpVelocity","jumpIceVelocity":"jumpIceVelocity","runForce":"runForce",
 "floatForce":"floatForce","defaultMaxSpeed":"defaultMaxSpeed","waterIceMaxSpeed":"waterIceMaxSpeed",
 "absoluteMaxSpeed":"absoluteMaxSpeed","feetFriction":"feetFriction","spriteScale":"spriteScale",
 "r1":"r1","r2":"r2","dr":"dr","y1":"y1","y2":"y2"}
bad = 0
for h, j in pairs.items():
    m = re.search(r"e\.prototype\.%s = (-?[0-9.]+)" % j, js)
    m2 = re.search(r"constexpr double %s\s*=\s*([+-]?[0-9.]+)" % h, hpp)
    if not m or not m2: print("MISSING", h); bad += 1; continue
    a, b = float(m.group(1)), float(m2.group(1))
    if h in ("y1", "y2"): b = -b                      # spec is y-down, original is mirrored
    ok = abs(a - b) < 1e-9
    print(("ok  " if ok else "BAD ") + f"{h}: init.js={a} spec={b}"); bad += (not ok)
# structural constants
for pat, label in [(r"categoryBits = 16, \w+\.filter\.maskBits = 4353", "char cat/mask 16/4353"),
                   (r"h\.density = 1\.4|density = 1\.4", "hull density 1.4"),
                   (r"new box2d\.b2CircleShape\(\.1\)", "portal sensor r=0.1"),
                   (r"categoryBits = 4096, \w+\.filter\.maskBits = 4096, \w+\.filter\.groupIndex = -1", "portal sensor filter 4096/4096/-1"),
                   (r"SetBullet\(!0\)", "bullet body"), (r"fixedRotation = !0", "fixed rotation")]:
    ok = re.search(pat, js) is not None; print(("ok  " if ok else "BAD ") + label); bad += (not ok)
print("ALL OK" if not bad else f"{bad} problems"); sys.exit(1 if bad else 0)
