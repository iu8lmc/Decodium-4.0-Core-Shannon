#!/usr/bin/env python3
"""Check that a Windows bundle directory has every DLL its binaries import.

Walks every .exe/.dll under the bundle (plugins and QML modules included), reads the
import and delay-import tables, and reports each imported DLL that is neither in the
application directory, the importing binary's directory or a Windows system DLL.
This checks the installed layout as well as the portable bundle; it cannot prove
arbitrary dynamically constructed LoadLibrary paths.

    python check-bundle-dlls.py <bundle-dir> [--exclude NAME ...] [--strings]

--exclude   file names the installer will not ship (they are removed from the bundle
            before checking, so a binary that still needs one is reported)
--orphans   list the DLLs in the bundle root that no executable, plugin or QML module
            reaches through its imports (candidates for the installer Excludes)
--strings   also list ".dll" names found as text inside the binaries (LoadLibrary by
            name) that are neither in the bundle nor a system DLL; informational only

Exit status 1 when an import is unresolved.
"""
import argparse
import os
import re
import struct
import sys

SYSTEM_DIRS = [os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), d) for d in ("System32", "SysWOW64")]
# Present on every supported Windows even when the check runs on another OS.
KNOWN_SYSTEM = {
    "kernel32.dll", "kernelbase.dll", "user32.dll", "gdi32.dll", "advapi32.dll", "shell32.dll",
    "ole32.dll", "oleaut32.dll", "ws2_32.dll", "msvcrt.dll", "ntdll.dll", "comdlg32.dll",
    "comctl32.dll", "winmm.dll", "version.dll", "imm32.dll", "shlwapi.dll", "setupapi.dll",
    "crypt32.dll", "bcrypt.dll", "secur32.dll", "rpcrt4.dll", "iphlpapi.dll", "dwmapi.dll",
    "uxtheme.dll", "opengl32.dll", "d3d9.dll", "d3d11.dll", "d3d12.dll", "dxgi.dll",
    "dnsapi.dll", "mswsock.dll", "netapi32.dll", "userenv.dll", "winspool.drv", "wtsapi32.dll",
    "shcore.dll", "dbghelp.dll", "psapi.dll", "powrprof.dll", "cfgmgr32.dll", "mpr.dll",
    "winhttp.dll", "wininet.dll", "mfplat.dll", "mf.dll", "mfreadwrite.dll", "mfuuid.dll",
    "avrt.dll", "ksuser.dll", "dwrite.dll", "d2d1.dll", "d3dcompiler_47.dll", "dxva2.dll",
    "evr.dll", "propsys.dll", "wlanapi.dll", "bthprops.cpl", "hid.dll", "normaliz.dll",
    "wsock32.dll", "oleacc.dll", "uiautomationcore.dll", "msimg32.dll", "ncrypt.dll",
    "sechost.dll", "cabinet.dll", "winusb.dll", "usp10.dll", "gdiplus.dll", "d3d10.dll",
    "authz.dll", "cryptsp.dll", "odbc32.dll", "pdh.dll",
}


def system_has(name):
    low = name.lower()
    if low.startswith(("api-ms-", "ext-ms-")) or low in KNOWN_SYSTEM:
        return True
    return any(os.path.isfile(os.path.join(d, name)) for d in SYSTEM_DIRS)


def read_cstr(data, off):
    end = data.find(b"\0", off)
    return data[off:end].decode("ascii", "replace") if end >= 0 else ""


def pe_imports(path):
    """Names of the DLLs a PE file imports (normal and delay-load)."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:2] != b"MZ":
        return None
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        return None
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    optsz = struct.unpack_from("<H", data, pe + 20)[0]
    opt = pe + 24
    magic = struct.unpack_from("<H", data, opt)[0]
    dd = opt + (112 if magic == 0x20B else 96)
    sections = []
    sec = opt + optsz
    for i in range(nsec):
        vsz, va, rsz, raw = struct.unpack_from("<IIII", data, sec + 40 * i + 8)
        sections.append((va, max(vsz, rsz), raw))

    def rva(r):
        for va, size, raw in sections:
            if va <= r < va + size:
                return r - va + raw
        return None

    names = []
    # Import directory (index 1): 20-byte descriptors, Name at +12.
    imp_rva, imp_size = struct.unpack_from("<II", data, dd + 8)
    off = rva(imp_rva) if imp_rva else None
    while off is not None and off + 20 <= len(data):
        desc = struct.unpack_from("<IIIII", data, off)
        if desc[3] == 0:
            break
        n = rva(desc[3])
        if n is not None:
            names.append(read_cstr(data, n))
        off += 20
    # Delay-import directory (index 13): 32-byte descriptors, Name RVA at +4.
    dl_rva, dl_size = struct.unpack_from("<II", data, dd + 13 * 8)
    off = rva(dl_rva) if dl_rva else None
    while off is not None and off + 32 <= len(data):
        attrs, name_rva = struct.unpack_from("<II", data, off)
        if name_rva == 0:
            break
        n = rva(name_rva)
        if n is not None:
            names.append(read_cstr(data, n))
        off += 32
    return names


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("bundle")
    ap.add_argument("--exclude", nargs="*", default=[])
    ap.add_argument("--strings", action="store_true")
    ap.add_argument("--orphans", action="store_true")
    args = ap.parse_args()

    root = os.path.abspath(args.bundle)
    excluded = {e.lower() for e in args.exclude}
    files = []
    for dp, _dn, fn in os.walk(root):
        for f in fn:
            if f.lower().endswith((".exe", ".dll")) and f.lower() not in excluded:
                files.append(os.path.join(dp, f))
    present = {os.path.basename(f).lower() for f in files}
    directories = {}
    for f in files:
        directories.setdefault(os.path.basename(f).lower(), set()).add(os.path.normcase(os.path.dirname(f)).lower())
    application_dir = os.path.normcase(root).lower()

    missing = {}
    unreadable = []
    for f in files:
        try:
            imports = pe_imports(f)
        except (OSError, struct.error):
            unreadable.append(os.path.relpath(f, root))
            continue
        if imports is None:
            unreadable.append(os.path.relpath(f, root))
            continue
        for name in imports or []:
            low = name.lower()
            importer_dir = os.path.normcase(os.path.dirname(f)).lower()
            visible = directories.get(low, set())
            if application_dir in visible or importer_dir in visible or system_has(name):
                continue
            missing.setdefault(name, set()).add(os.path.relpath(f, root))

    print("Bundle: %s  (%d binaries checked, %d names excluded)" % (root, len(files), len(excluded)))
    if unreadable:
        print("\nINVALID OR UNREADABLE PE BINARIES:")
        for name in sorted(unreadable):
            print("  " + name)
    if missing:
        print("\nUNRESOLVED IMPORTS:")
        for name in sorted(missing, key=str.lower):
            users = sorted(missing[name])
            print("  %-34s needed by %s%s" % (name, ", ".join(users[:4]), " ..." if len(users) > 4 else ""))
    else:
        if not unreadable:
            print("All imports resolve to accessible bundle locations or Windows system DLLs.")

    if args.orphans:
        by_name = {os.path.basename(f).lower(): f for f in files if os.path.dirname(f) == root}
        imports_of = {f: [n.lower() for n in (pe_imports(f) or [])] for f in files}
        reached = set()
        stack = [f for f in files if f.lower().endswith(".exe") or os.path.dirname(f) != root]
        while stack:
            f = stack.pop()
            if f in reached:
                continue
            reached.add(f)
            stack.extend(by_name[n] for n in imports_of[f] if n in by_name and by_name[n] not in reached)
        orphans = sorted((os.path.getsize(f), os.path.basename(f)) for f in files if f not in reached)
        print("\nDLLs in the bundle root that nothing reaches:")
        for size, name in reversed(orphans):
            print("  %6.1f MB  %s" % (size / 1048576.0, name))
        print("  total %.1f MB in %d files" % (sum(s for s, _ in orphans) / 1048576.0, len(orphans)))

    if args.strings:
        pat = re.compile(rb"[A-Za-z0-9_.+-]{3,60}\.dll", re.I)
        seen = {}
        for f in files:
            if not f.lower().endswith(".exe") and os.path.dirname(f) != root:
                continue
            with open(f, "rb") as fh:
                blob = fh.read()
            for m in set(pat.findall(blob)):
                nm = m.decode("ascii").strip()
                low = nm.lower()
                if low in present or system_has(nm) or low in excluded:
                    continue
                seen.setdefault(nm, set()).add(os.path.basename(f))
        print("\nDLL names found as text, not in the bundle and not system (informational):")
        for nm in sorted(seen, key=str.lower):
            print("  %-34s in %s" % (nm, ", ".join(sorted(seen[nm])[:3])))

    return 1 if missing or unreadable else 0


if __name__ == "__main__":
    sys.exit(main())
