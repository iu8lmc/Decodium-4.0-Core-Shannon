#!/usr/bin/env python3
"""Inspect the GCC target pragma in cross-compiled Windows COFF objects.

Requires x86_64-w64-mingw32-g++/nm/objdump. Does not execute Windows code.
"""
import pathlib
import re
import shutil
import subprocess
import tempfile


def run(*args):
    return subprocess.run(args, check=True, text=True, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE).stdout


def main():
    prefix = "x86_64-w64-mingw32-"
    for name in ("g++", "nm", "objdump"):
        if not shutil.which(prefix + name):
            raise SystemExit(f"Required tool missing: {prefix + name}")
    with tempfile.TemporaryDirectory(prefix="decodium-avx2-mingw-") as temp:
        p = pathlib.Path(temp)
        pragma = '''#pragma GCC push_options\n#pragma GCC target("no-avx,no-avx2,no-fma")\n#include <bits/stdc++.h>\n#pragma GCC pop_options\n'''
        source = '''#include <vector>\n#include <immintrin.h>\ntemplate class std::vector<int>;\nextern "C" int fast() { std::vector<int> v(128); v.assign(128,17); return v[42]; }\nextern "C" __m256i vector_add(__m256i a, __m256i b) { return _mm256_add_epi32(a,b); }\n'''
        (p / "before.cpp").write_text(source)
        (p / "after.cpp").write_text(pragma + source)
        for name in ("before", "after"):
            run(prefix + "g++", "-O3", "-std=c++17", "-mavx2", "-mfma", "-c",
                str(p / f"{name}.cpp"), "-o", str(p / f"{name}.o"))
        lines = run(prefix + "nm", "--defined-only", str(p / "before.o")).splitlines()
        symbols = [line.split()[-1] for line in lines
                   if "_M_fill_assign" in line and line.split()[-1].startswith("_ZN")]
        if len(symbols) != 1:
            raise SystemExit(f"Expected one vector fill symbol, found: {symbols}")
        symbol = symbols[0]
        def disassemble(name, function):
            return run(prefix + "objdump", "-d", "--disassemble=" + function, str(p / f"{name}.o"))
        before = disassemble("before", symbol)
        after = disassemble("after", symbol)
        if "vpbroadcastd" not in before:
            raise SystemExit("Expected AVX2 instruction absent before patch: inconclusive")
        if re.search(r"\t(v[a-z][a-z0-9]*)\s", after):
            raise SystemExit("AVX/AVX2 instructions remain in vector fill after pragma")
        if "vpaddd" not in disassemble("after", "vector_add"):
            raise SystemExit("AVX2 backend was not restored after pragma")
        print(f"PASS: {symbol}: AVX2 before, baseline after; SIMD backend remains AVX2")
        # Repeat on the actual decoder translation unit, not just the small
        # collision reproducer. The source before the fix is recreated only
        # in the disposable directory; the working tree is never modified.
        source_dir = pathlib.Path(__file__).resolve().parents[1] / "Detector/superldpc"
        actual = p / "real"
        shutil.copytree(source_dir, actual)
        real_source = (actual / "decodium_bridge.cpp").read_text()
        begin = real_source.index("#if defined(__GNUC__) && !defined(__clang__)")
        end = real_source.index("#endif", begin) + len("#endif")
        (actual / "before.cpp").write_text(real_source[:begin] + real_source[end:])
        for name in ("before", "decodium_bridge"):
            run(prefix + "g++", "-O3", "-std=c++17", "-mavx2", "-mfma", "-Wall", "-Wextra", "-Werror",
                "-DSUPERLDPC_HAVE_GATE_WEIGHTS=1", "-c", str(actual / f"{name}.cpp"),
                "-o", str(actual / f"{name}.o"))
        actual_symbols = run(prefix + "nm", "--defined-only", str(actual / "before.o")).splitlines()
        candidates = [line.split()[-1] for line in actual_symbols if "_M_fill_assign" in line
                      and line.split()[-1].startswith("_ZNSt6vectorIi")]
        if len(candidates) != 1:
            raise SystemExit(f"Expected one real decoder int vector fill symbol, found: {candidates}")
        real_symbol = candidates[0]
        before_real = run(prefix + "objdump", "-d", "--disassemble=" + real_symbol, str(actual / "before.o"))
        after_real = run(prefix + "objdump", "-d", "--disassemble=" + real_symbol, str(actual / "decodium_bridge.o"))
        if not re.search(r"\t(v[a-z][a-z0-9]*)\s", before_real):
            raise SystemExit("Expected AVX instructions absent from real vector fill before fix")
        if re.search(r"\t(v[a-z][a-z0-9]*)\s", after_real):
            raise SystemExit("AVX instructions remain in real vector fill after fix")
        if "vpaddd" not in run(prefix + "objdump", "-d", str(actual / "decodium_bridge.o")):
            raise SystemExit("Real decoder no longer contains expected AVX2 backend code")
        print("PASS: real decoder TU compiles with -Werror; vector fill is baseline and backend retains AVX2")
        print("Object-file inspection only: no Windows executable was run")


if __name__ == "__main__":
    main()
