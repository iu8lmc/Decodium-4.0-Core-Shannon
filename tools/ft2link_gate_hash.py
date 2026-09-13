#!/usr/bin/env python3
"""Generate build-time FT2-Link access gate parameters.

The password is read with getpass and is never printed. The generated salt and
PBKDF2-HMAC-SHA256 hash are intended to be passed to CMake with -D options.
"""

import argparse
import base64
import getpass
import hashlib
import os
import secrets
import shlex
import subprocess
import sys


DEFAULT_ITERATIONS = 160000
SALT_BYTES = 16
HASH_BYTES = 32

# On Windows with MSYS2/MinGW64 toolchain, cmake lives in the MSYS2 prefix.
_MSYS2_CMAKE = r"C:\msys64\mingw64\bin\cmake.exe"


def _cmake_exe():
    """Return the cmake executable to use, preferring MSYS2 on Windows."""
    if sys.platform.startswith("win") and os.path.isfile(_MSYS2_CMAKE):
        return _MSYS2_CMAKE
    return "cmake"


def default_build_dir():
    if sys.platform.startswith("win"):
        return "build_mingw64"
    if sys.platform == "darwin":
        return "build-local-macos"
    return "build-local-linux"


def shell_join(parts):
    return " ".join(shlex.quote(part) for part in parts)


def main():
    parser = argparse.ArgumentParser(
        description="Generate sealed FT2-Link access gate salt/hash for CMake."
    )
    parser.add_argument(
        "--iterations",
        type=int,
        default=DEFAULT_ITERATIONS,
        help=f"PBKDF2 iterations, minimum 10000 (default: {DEFAULT_ITERATIONS})",
    )
    parser.add_argument(
        "--min-length",
        type=int,
        default=8,
        help="minimum accepted password length (default: 8)",
    )
    parser.add_argument(
        "--source-dir",
        default=".",
        help="CMake source directory used with --apply (default: .)",
    )
    parser.add_argument(
        "--build-dir",
        default=default_build_dir(),
        help="CMake build directory used with --apply/--build (default: platform-specific)",
    )
    parser.add_argument(
        "--apply",
        action="store_true",
        help="run cmake -S/-B with the generated salt/hash",
    )
    parser.add_argument(
        "--build",
        action="store_true",
        help="after --apply, build the selected target",
    )
    parser.add_argument(
        "--target",
        default="decodium_qml",
        help="target to build with --build (default: decodium_qml)",
    )
    args = parser.parse_args()

    if args.iterations < 10000:
        print("iterations must be at least 10000", file=sys.stderr)
        return 2

    password = getpass.getpass("FT2-Link password: ")
    if len(password) < args.min_length:
        print(f"password must be at least {args.min_length} characters", file=sys.stderr)
        return 2

    confirm = getpass.getpass("Confirm password: ")
    if password != confirm:
        print("passwords do not match", file=sys.stderr)
        return 2

    salt = secrets.token_bytes(SALT_BYTES)
    digest = hashlib.pbkdf2_hmac(
        "sha256",
        password.encode("utf-8"),
        salt,
        args.iterations,
        dklen=HASH_BYTES,
    )

    salt_b64 = base64.b64encode(salt).decode("ascii")
    hash_b64 = base64.b64encode(digest).decode("ascii")

    print("Generated FT2-Link access gate parameters.")
    print("The password was not printed. Do not commit these generated values.")
    print()
    print(f"DECODIUM_FT2LINK_ACCESS_SALT_B64={salt_b64}")
    print(f"DECODIUM_FT2LINK_ACCESS_HASH_B64={hash_b64}")
    print(f"DECODIUM_FT2LINK_ACCESS_ITERATIONS={args.iterations}")
    print()
    cmake_exe = _cmake_exe()
    cmake_cmd = [
        cmake_exe,
        "-S",
        args.source_dir,
        "-B",
        args.build_dir,
        f"-DDECODIUM_FT2LINK_ACCESS_SALT_B64={salt_b64}",
        f"-DDECODIUM_FT2LINK_ACCESS_HASH_B64={hash_b64}",
        f"-DDECODIUM_FT2LINK_ACCESS_ITERATIONS={args.iterations}",
    ]

    print("CMake example:")
    print(shell_join(cmake_cmd))

    if args.apply:
        print()
        print("Applying generated parameters to CMake cache...")
        subprocess.run(cmake_cmd, check=True)

    if args.build:
        if not args.apply:
            print("--build requires --apply so the generated values are configured", file=sys.stderr)
            return 2
        jobs = str(max(1, os.cpu_count() or 1))
        build_cmd = [
            cmake_exe,
            "--build",
            args.build_dir,
            "--target",
            args.target,
            "-j",
            jobs,
        ]
        print()
        print("Building target:")
        print(shell_join(build_cmd))
        subprocess.run(build_cmd, check=True)

    if args.apply:
        print()
        print(
            "Done. Restart Decodium from this build output before testing FT2-Link."
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
