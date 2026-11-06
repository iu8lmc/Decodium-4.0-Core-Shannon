#!/usr/bin/env python3
"""Require signed two-application A/B security tests in macOS CTest inventory."""
import json
import sys

CASES = ("replay_resigned", "replay_index_upgrade", "invalid_then_valid",
         "nonce_restart", "legacy_invalid", "corrupt_recipient_pin", "corrupt_sender_pin",
         "expiry_awaiting", "inbox_unavailable", "expiry_pending_restart")
REQUIRED = {f"test_sstv_share_app_signed_{case}_{variant}"
            for case in CASES for variant in ("a", "b")}
LABELS = {"sstv", "sharing", "security", "application"}


def check(data):
    if not isinstance(data, dict) or not isinstance(data.get("tests"), list):
        raise ValueError("invalid CTest inventory")
    tests = {}
    for test in data["tests"]:
        if not isinstance(test, dict) or not isinstance(test.get("name"), str):
            raise ValueError("invalid CTest test entry")
        if test["name"] in tests:
            raise ValueError("duplicate CTest test name: " + test["name"])
        tests[test["name"]] = test
    missing = sorted(REQUIRED - tests.keys())
    if missing:
        raise ValueError("missing signed sharing tests: " + ", ".join(missing))
    for name in sorted(REQUIRED):
        properties = tests[name].get("properties")
        if not isinstance(properties, list):
            raise ValueError("missing CTest properties: " + name)
        label_values = [p.get("value") for p in properties
                        if isinstance(p, dict) and p.get("name") == "LABELS"]
        if (len(label_values) != 1 or not isinstance(label_values[0], list)
                or not all(isinstance(label, str) for label in label_values[0])
                or not LABELS.issubset(label_values[0])):
            raise ValueError("missing CTest labels: " + name)
    return len(REQUIRED)


def main():
    try:
        count = check(json.load(sys.stdin))
    except (ValueError, json.JSONDecodeError) as exc:
        print(str(exc), file=sys.stderr)
        return 1
    print(f"macOS inventory contains all {count} signed sharing A/B tests")
    return 0


if __name__ == "__main__":
    sys.exit(main())
