#!/usr/bin/env python3
"""A/B positive and negative controls for the macOS signed-test inventory."""
import copy
import json
import subprocess
import sys
from pathlib import Path

from check_macos_share_tests import LABELS, REQUIRED

script = Path(__file__).with_name("check_macos_share_tests.py")
def run(payload):
    return subprocess.run([sys.executable, str(script)], input=json.dumps(payload),
                          text=True, capture_output=True)

for side in ("a", "b"):
    tests = [{"name": name, "properties": [{"name": "LABELS", "value": sorted(LABELS)}]}
             for name in sorted(REQUIRED)]
    tests.append({"name": "additional_test_" + side, "properties": []})
    assert run({"tests": tests}).returncode == 0
    missing = "test_sstv_share_app_signed_nonce_restart_" + side
    assert run({"tests": [test for test in tests if test["name"] != missing]}).returncode == 1
    altered = copy.deepcopy(tests)
    target = next(test for test in altered if test["name"] == missing)
    target["properties"][0]["value"].remove("sstv")
    assert run({"tests": altered}).returncode == 1
    target["properties"][0]["value"].append({})
    assert run({"tests": altered}).returncode == 1
    assert run({"tests": tests + [copy.deepcopy(tests[0])]}).returncode == 1
assert run({}).returncode == 1
print("PASS macOS signed sharing inventory A/B and negative controls")
