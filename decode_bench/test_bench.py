"""Regression checks for failures that must invalidate a benchmark run."""

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import bench
import compare_builds
from compare_builds import parse_stage4


class DecoderProcessTests(unittest.TestCase):
    def test_successful_empty_output_is_a_valid_no_decode(self):
        self.assertEqual(bench.run_decodium([sys.executable, "-c", "pass"], 5), "")

    def test_nonzero_exit_is_not_a_no_decode(self):
        with self.assertRaisesRegex(RuntimeError, "exit 3.*bad wav"):
            bench.run_decodium(
                [sys.executable, "-c", "import sys; print('bad wav', file=sys.stderr); sys.exit(3)"], 5)

    def test_timeout_is_not_a_no_decode(self):
        with patch("bench.subprocess.run", side_effect=subprocess.TimeoutExpired("decoder", 1)):
            with self.assertRaisesRegex(RuntimeError, "scaduto.*slot.wav"):
                bench.run_decodium(["decoder", "slot.wav"], 1)


class StageComparisonTests(unittest.TestCase):
    def test_same_message_at_two_frequencies_remains_two_decodes(self):
        result = parse_stage4('stage 4: nout=2\n'
                              '  - snr= -5 dt=0.1 freq= 1500 nap= 0 qual=1.000 decoded="CQ K1ABC FN20"\n'
                              '  - snr= -6 dt=0.1 freq= 1600 nap= 0 qual=1.000 decoded="CQ K1ABC FN20"\n')
        self.assertEqual(sum(result.values()), 2)
        self.assertEqual(len(result), 2)

    def test_missing_or_incomplete_stage_is_rejected(self):
        for output in ("", "stage 4: nout=1\n", "stage 4: nout=1\n  (no decodes)\n"):
            with self.subTest(output=output), self.assertRaises(ValueError):
                parse_stage4(output)

    def test_empty_stage_is_valid(self):
        self.assertFalse(parse_stage4("stage 4: nout=0\n  (no decodes)\n"))

    def test_ft2_stage_seven_is_selected(self):
        self.assertFalse(parse_stage4("stage 7: nout=0\n  (no decodes)\n", 7))
        with self.assertRaises(ValueError):
            parse_stage4("stage 4: nout=0\n  (no decodes)\n", 7)

    def test_comparison_reports_missing_decode_and_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            baseline = root / "baseline"
            candidate = root / "candidate"
            baseline.write_text("baseline")
            candidate.write_text("candidate")
            (root / "slot.wav").write_bytes(b"fixture")
            output = root / "result.json"
            before = ('stage 4: nout=1\n'
                      '  - snr=0 dt=0.1 freq=1500 nap=0 qual=1.000 decoded="CQ K1ABC FN20"\n')
            after = 'stage 4: nout=0\n  (no decodes)\n'
            args = ["compare_builds", "--baseline", str(baseline), "--candidate", str(candidate),
                    "--wavs", str(root), "--output", str(output)]
            with patch.object(sys, "argv", args), \
                 patch("compare_builds.run_decodium", side_effect=[before, after]):
                self.assertEqual(compare_builds.main(), 1)
            self.assertIn('"message": "CQ K1ABC FN20"', output.read_text())

    def test_explicit_depth_reaches_both_decoders(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in ("baseline", "candidate", "slot.wav"):
                (root / name).write_text(name)
            for mode, stage, option in (("ft8", 4, "--depth"), ("ft2", 7, "--ndepth")):
                args = ["compare_builds", "--mode", mode, "--profile", "prod", "--depth", "2",
                        "--baseline", str(root / "baseline"), "--candidate", str(root / "candidate"),
                        "--wavs", str(root / "slot.wav"), "--output", str(root / "result.json")]
                with self.subTest(mode=mode), patch.object(sys, "argv", args), \
                     patch("compare_builds.run_decodium", return_value=f"stage {stage}: nout=0\n") as run:
                    self.assertEqual(compare_builds.main(), 0)
                    self.assertEqual(run.call_count, 2)
                    for call in run.call_args_list:
                        command = call.args[0]
                        self.assertEqual(command[command.index(option) + 1], "2")


if __name__ == "__main__":
    unittest.main()
