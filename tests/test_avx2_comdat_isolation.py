#!/usr/bin/env python3
"""Reproduce and check GCC's cross-ISA COMDAT collision on Linux x86-64.

Run with: python3 tests/test_avx2_comdat_isolation.py
Requires g++ and binutils, plus either a CPU without AVX2 or qemu-x86_64.
Builds only disposable files in /tmp.
This is a minimal reproducer, not a substitute for starting the real app.
"""
import pathlib
import resource
import shutil
import subprocess
import sys
import tempfile


def run(*args, check=True):
    return subprocess.run(args, text=True, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, check=check)


def main():
    if sys.platform != "linux":
        raise SystemExit("This ELF/GCC test must run on Linux x86-64")
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    for tool in ("g++", "nm", "objdump"):
        if not shutil.which(tool):
            raise SystemExit(f"Required tool missing: {tool}")
    if shutil.which("qemu-x86_64"):
        runner = ("qemu-x86_64", "-cpu", "IvyBridge")
    elif " avx2 " not in " " + pathlib.Path("/proc/cpuinfo").read_text() + " ":
        runner = ()
    else:
        raise SystemExit("Need qemu-x86_64 or a real CPU without AVX2")
    with tempfile.TemporaryDirectory(prefix="decodium-avx2-comdat-") as temp:
        p = pathlib.Path(temp)
        # Force emission of libstdc++'s real weak vector fill method in the
        # AVX2 translation unit; baseline JTTY-like code calls the same method.
        fast = '''#include <vector>\ntemplate class std::vector<int>;\nextern "C" int fast() { std::vector<int> v(128); v.assign(128,17); return v[42]; }\n'''
        base = '''#include <vector>\nextern "C" int base() { std::vector<int> v(128); v.assign(128,17); return v[42]; }\n'''
        pragma = '''#pragma GCC push_options\n#pragma GCC target("no-avx,no-avx2,no-fma")\n#include <bits/stdc++.h>\n#pragma GCC pop_options\n'''
        (p / "fast.cpp").write_text(fast)
        (p / "fast-isolated.cpp").write_text(pragma + fast)
        (p / "base.cpp").write_text(base)
        (p / "main.cpp").write_text('extern "C" int base(); int main(){return base()==17 ? 0 : 1;}\n')
        common = ("-O3", "-std=c++17", "-ffunction-sections")
        for name, flags in (("fast", ("-mavx2",)), ("base", ("-fno-inline",)), ("main", ())):
            run("g++", *common, *flags, "-c", str(p / f"{name}.cpp"), "-o", str(p / f"{name}.o"))
        symbols = run("nm", "--defined-only", str(p / "fast.o")).stdout.splitlines()
        matches = [line.split()[-1] for line in symbols
                   if "_M_fill_assign" in line and len(line.split()) >= 3
                   and line.split()[-2] in ("W", "V")]
        if len(matches) != 1:
            raise SystemExit(f"Expected one weak vector fill symbol, found: {matches}")
        symbol = matches[0]
        fast_disassembly = run("objdump", "-d", "--disassemble=" + symbol, str(p / "fast.o")).stdout
        if "vpbroadcastd" not in fast_disassembly:
            raise SystemExit("Compiler did not emit the expected AVX2 instruction; reproducer is inconclusive")
        def link(output, obj):
            run("g++", str(obj), str(p / "base.o"), str(p / "main.o"), "-o", str(p / output))
        link("before", p / "fast.o")
        run("g++", *common, "-mavx2", "-c", str(p / "fast-isolated.cpp"), "-o", str(p / "fast-isolated.o"))
        isolated_disassembly = run("objdump", "-d", "--disassemble=" + symbol,
                                   str(p / "fast-isolated.o")).stdout
        if "vpbroadcastd" in isolated_disassembly:
            raise SystemExit("GCC target pragma did not remove AVX2 from vector fill")
        link("after", p / "fast-isolated.o")
        # Ivy Bridge supports AVX but not AVX2 or FMA, matching the reporter.
        before = run(*runner, str(p / "before"), check=False)
        after = run(*runner, str(p / "after"), check=False)
        print(f"symbol={symbol}; before exit={before.returncode}; after exit={after.returncode}")
        if before.returncode not in (-4, 132):
            raise SystemExit("Before build did not fail with SIGILL: reproducer inconclusive")
        if after.returncode != 0:
            raise SystemExit("Isolated build did not run successfully on the test CPU")
        print("PASS: weak AVX2 collision reproduced and target pragma avoided SIGILL")


if __name__ == "__main__":
    main()
