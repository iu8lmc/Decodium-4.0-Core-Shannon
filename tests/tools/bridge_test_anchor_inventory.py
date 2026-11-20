#!/usr/bin/env python3
"""Find potentially stale Bridge test insertion anchors and helper wiring.

This is a heuristic: it examines Python `point`/`anchor` string assignments
where the script also calls `m.count()` for that name. It cannot prove that a
test otherwise builds or runs, or find dynamically constructed anchors. The
FT8 fixture check is specific to scripts that reinsert that fixture at the
UTC timer marker; it is not a general C++ dependency checker.
The courtesy check similarly applies only when that fixture is inserted into
the main Bridge file. Neither check validates the behaviour of its forwarder.
Constructor call anchors are checked when a script selects the old or split
constructor file and passes a literal or named point to m.count().
"""
import ast
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MAIN = (ROOT / "src/bridge/DecodiumBridge.cpp").read_text()
CONSTRUCTOR = (ROOT / "src/bridge/DecodiumBridgeConstructor.cpp").read_text()
FT8_FORWARDERS = (
    "#define appendDecodeTextBatchesAsync",
    "#define decodeDedupKey",
    "#define decodeMirrorSemanticKey",
    "#define adaptiveInteractiveFtThreadCount",
)


def missing_ft8_forwarders(text):
    if ("ft8_ready_before.inc" not in text
            or "BRIDGE_UTC_TIMER_INSERTION_POINT" not in text
            or "main.write_text" not in text):
        return ()
    return tuple(item for item in FT8_FORWARDERS if item not in text)


def missing_courtesy_forwarder(text):
    if ("courtesy_before.inc" not in text
            or "BRIDGE_LAB_INSERTION_POINT" not in text
            or "main.write_text" not in text):
        return False
    return not ("fixture.replace('kRecentAutoCqPartnerWindowSeconds'" in text
                and 'int decodiumBridgeRecentAutoCqPartnerWindowSeconds();' in text)


def main():
    missing = []
    missing_forwarders = []
    missing_courtesy = []
    missing_constructor_calls = []
    for path in sorted((ROOT / "tests/tools").glob("*.py")):
        text = path.read_text()
        absent = missing_ft8_forwarders(text)
        if absent:
            missing_forwarders.append((path.name, absent))
        if missing_courtesy_forwarder(text):
            missing_courtesy.append(path.name)
        try:
            tree = ast.parse(text)
        except SyntaxError:
            continue
        if "main=" in text.replace(" ", "") and "src/bridge/DecodiumBridge" in text:
            source = (CONSTRUCTOR if "'src/bridge/DecodiumBridgeConstructor.cpp'" in text
                      else MAIN)
            points = {node.targets[0].id: node.value.value for node in ast.walk(tree)
                      if isinstance(node, ast.Assign) and len(node.targets) == 1
                      and isinstance(node.targets[0], ast.Name)
                      and isinstance(node.value, ast.Constant)
                      and isinstance(node.value.value, str)}
            for node in ast.walk(tree):
                if not (isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)
                        and isinstance(node.func.value, ast.Name) and node.func.value.id == 'm'
                        and node.func.attr == 'count' and node.args):
                    continue
                arg = node.args[0]
                anchor = (arg.value if isinstance(arg, ast.Constant) else
                          points.get(arg.id) if isinstance(arg, ast.Name) else None)
                if (isinstance(anchor, str)
                        and re.fullmatch(r'\s*initialise\w+\(\);\s*', anchor)
                        and anchor not in source):
                    missing_constructor_calls.append((path.name, anchor.strip()))
        for node in ast.walk(tree):
            if not (isinstance(node, ast.Assign) and len(node.targets) == 1
                    and isinstance(node.targets[0], ast.Name)
                    and node.targets[0].id in {"point", "anchor"}
                    and isinstance(node.value, ast.Constant)
                    and isinstance(node.value.value, str)):
                continue
            name, anchor = node.targets[0].id, node.value.value
            if ("DecodiumBridge::" in anchor and anchor not in MAIN
                    and re.search(r"\bm\.count\(\s*" + name + r"\s*\)", text)):
                missing.append((path.name, anchor))
    for filename, anchor in missing:
        print(f"{filename}: {anchor!r}")
    print(f"Potentially stale insertion anchors: {len(missing)}")
    for filename, absent in missing_forwarders:
        print(f"{filename}: missing FT8 fixture forwards: {', '.join(absent)}")
    print(f"FT8 fixture scripts with missing forwards: {len(missing_forwarders)}")
    for filename in missing_courtesy:
        print(f"{filename}: missing courtesy window forwarder")
    print(f"Courtesy fixture scripts with missing forwards: {len(missing_courtesy)}")
    for filename, anchor in missing_constructor_calls:
        print(f"{filename}: missing constructor call {anchor}")
    print(f"Potentially stale constructor call anchors: {len(missing_constructor_calls)}")


if __name__ == "__main__":
    main()
