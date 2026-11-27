#!/usr/bin/env python3
"""Create a searchable index of possible Bridge test evidence.

Text matches are discovery aids, not proof of execution, A/B comparison, or
coverage. In particular, inline methods, overloads and indirect calls require
manual review. The index deliberately includes only explicitly named methods.
"""
import json
import re
from pathlib import Path
from bridge_source_method_diff import method_bodies

ROOT = Path(__file__).resolve().parents[2]
IDENTIFIER = re.compile(r"\b[A-Za-z_]\w*\b")


def main():
    modules = sorted((ROOT / "src/bridge").glob("DecodiumBridge*.cpp"))
    tests = sorted(p for p in (ROOT / "tests").rglob("*")
                   if p.is_file() and p.suffix in {".py", ".cpp", ".h"})
    index = {}
    for path in tests:
        contents = path.read_text(errors="replace")
        for word in set(IDENTIFIER.findall(contents)):
            index.setdefault(word, []).append(str(path.relative_to(ROOT)))
    result = []
    for module in modules:
        # Only index apparent definitions, not qualified calls such as
        # DecodiumBridge::tr() or a static metric written in another module.
        # This remains a textual C++ heuristic, not a full parser.
        methods = sorted(method_bodies(module.read_text(errors="replace")))
        result.append({
            "module": str(module.relative_to(ROOT)),
            "methods": [{"name": name, "mentions": index.get(name.lstrip("~"), [])}
                        for name in methods],
        })
    out = ROOT / "tests/bridge_refactor_evidence/bridge_evidence_inventory.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps({"warning": __doc__, "modules": result}, indent=2) + "\n")
    methods = [method for module in result for method in module["methods"]]
    print(f"Modules: {len(result)}; module/method name pairs: {len(methods)}")
    print(f"Without a textual mention in scanned tests: "
          f"{sum(not method['mentions'] for method in methods)}")
    print(f"Index: {out.relative_to(ROOT)}")
    print("Counts are discovery hints only; they are not test coverage results.")


if __name__ == "__main__":
    main()
