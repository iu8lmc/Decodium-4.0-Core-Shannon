# Bridge refactor completion audit

The maintained, more detailed audit is `completion_audit.md` in this directory.
This file is an earlier snapshot and should not be used alone for current status.

Scope: split Decodium Bridge into smaller files while preserving behavior, with A/B and functional evidence for touched functions. This audit does not change that scope. Native execution on every platform is not a goal gate, per the user's instruction; portable code remains required.

| Requirement | Current evidence | Assessment |
| --- | --- | --- |
| Split the main Bridge | `src/bridge/DecodiumBridge.cpp` is 700 lines in the current worktree; 410 `DecodiumBridge*.cpp` files have build rules in both configured macOS application targets. | Major extraction implemented; size alone does not validate maintainability or behavior. |
| Build the extracted modules | Both `wsjtx` and `decodium_qml` macOS targets built successfully on 2026-11-10. | Verified for the configured macOS build; portable source still requires review. |
| A/B comparisons for touched functions | `tests/run_bridge_refactor_ab.py`, focused scripts and evidence documented in `tests/BRIDGE_REFACTOR_AB.md`; many compare saved pre-extraction fixtures as the worktree included changes after v1.0.737. | Broad evidence exists; an exhaustive function-to-test execution audit is not yet established. Name matches and fixture equality are not runtime evidence. |
| Functional tests for touched functions | Actual isolated app probes and two-app BlackHole tests are documented in `tests/BRIDGE_REFACTOR_AB.md`. | Broad evidence exists; it does not cover every function and branch, and virtual audio does not prove hardware/RF. |
| Decode-path regression investigation | The independently verified JT65 reference initially failed in both placements; after correcting the native filter FFT direction, both placements decoded the expected message. FST4-15 clean input decoded in both Bridge placements and official WSJT-X at 1480–1550 Hz; all failed at 1480–1530 Hz in the tested configuration. | JT65 forced native snapshot/publication passes for the reference recording; live reception and the numerical comparator difference are not proven. The FST4 frequency-bound sensitivity is shared with the official control and remains unexplained. |
| Worktree safety | `git diff --check` passes; the worktree includes extensive tracked and untracked user work. | Changes must remain intact until independently reviewed or explicitly committed. |

The filename inventory and 26-name token review are discovery aids, not an exhaustive count of tests remaining. See the detailed report and evidence files for test-specific limits. Do not mark the goal complete until the uncovered or uncertain scope is resolved with evidence.
