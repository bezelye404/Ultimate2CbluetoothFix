#!/usr/bin/env python3
"""Independent reference for the core tests.

Written from the formulas of the Windows version, NOT from the C++ code in core/src.
It emulates 32-bit floats by rounding to float32 after every operation (+, -, *, / and sqrt are exact this way,
because a double result rounded to float32 equals the float32 result for these operations).

Output: CSV files in core/tests/vectors/. The C++ port must produce the same numbers.
It does NOT prove equality with the real Windows program (no Windows output was captured).
"""
import math
import os
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "vectors")


def f32(x):
    return struct.unpack("f", struct.pack("f", x))[0]


def trunc(x):  # C++ static_cast<int> of a float: toward zero
    return int(x)


def normalize_axis(v):
    c = v - 32767
    return max(-32768, min(32767, c))


def apply_deadzone(v, dz):
    if dz <= 0:
        return v
    if -dz < v < dz:
        return 0
    return v


def apply_curve(v, curve):
    if curve == 0 or v == 0:
        return v
    norm = f32(f32(float(v)) / f32(32767.0))
    sign = f32(1.0) if norm >= 0.0 else f32(-1.0)
    a = f32(abs(norm))
    if a > 1.0:
        a = f32(1.0)
    r = a
    if curve == 1:
        t = f32(f32(2.0) * a)
        t = f32(f32(3.0) - t)
        r = f32(f32(a * a) * t)
    elif curve == 2:
        r = f32(math.sqrt(a))
    res = trunc(f32(f32(sign * r) * f32(32767.0)))
    return max(-32768, min(32767, res))


def negate(v):
    return 32767 if v == -32768 else -v


def stick(raw, dz, curve, invert):
    v = apply_curve(apply_deadzone(normalize_axis(raw), dz), curve)
    return negate(v) if invert else v


def trigger_windows(axis, idle, click, hair):
    if click:
        return 255
    diff = axis - idle if axis >= idle else idle - axis
    if diff < 1500:
        return 0
    if hair:
        return 255
    span = 65535 - idle if idle <= 32768 else idle
    if span < 1000:
        span = 65535
    scaled = (diff * 255) // span
    return max(0, min(255, scaled))


def main():
    os.makedirs(OUT, exist_ok=True)
    raws = {v * 257 for v in range(256)} | set(range(0, 65536, 251)) | {0, 1, 32766, 32767, 32768, 32769, 65534, 65535}
    raws = sorted(raws)
    dzs = [0, 2600, 4000, 6500]
    with open(os.path.join(OUT, "axes.csv"), "w") as f:
        f.write("raw16,deadzone,curve,x_out,y_out\n")
        for raw in raws:
            for dz in dzs:
                for curve in (0, 1, 2):
                    f.write(f"{raw},{dz},{curve},{stick(raw, dz, curve, False)},{stick(raw, dz, curve, True)}\n")

    idles = [0, 2570, 32767, 32768, 40000, 65535]
    # the dead-zone edge (difference 1499, 1500, 1501 from every idle value) is the interesting place: include it explicitly
    edge = {i + d for i in idles for d in (-1501, -1500, -1499, 1499, 1500, 1501) if 0 <= i + d <= 65535}
    axes = sorted({v * 257 for v in range(256)} | set(range(0, 65536, 251)) | {0, 65535} | edge)
    with open(os.path.join(OUT, "triggers_windows.csv"), "w") as f:
        f.write("axis16,idle16,click,hair,out\n")
        for axis in axes:
            for idle in idles:
                for click in (0, 1):
                    for hair in (0, 1):
                        f.write(f"{axis},{idle},{click},{hair},{trigger_windows(axis, idle, bool(click), bool(hair))}\n")

    with open(os.path.join(OUT, "triggers_analog.csv"), "w") as f:
        f.write("v8,click,hair,analog_present,out\n")
        for v8 in range(256):
            for click in (0, 1):
                for hair in (0, 1):
                    for present in (1, 0):
                        if present:
                            out = trigger_windows(v8 * 257, 0, False, bool(hair))   # the click is ignored when analog is present
                        else:
                            out = 255 if click else 0
                        f.write(f"{v8},{click},{hair},{present},{out}\n")
    print("vectors written to", OUT)


if __name__ == "__main__":
    main()
