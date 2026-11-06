"""Validate the RX audio diagnostics used by the local SSTV laboratories."""
import json
import math


def clean_hamdrm_rx_diagnostics(log, source_device='BlackHole 2ch'):
    marker = '[LAB][SSTV] RX diagnostics '
    rows = [json.loads(line.split(marker, 1)[1]) for line in log.splitlines()
            if marker in line]
    counters = ('discontinuities', 'droppedChunks', 'hamdrmRejectedChunks',
                'hamdrmQueueOverflows', 'processingFailures')
    return bool(rows) and all(row.get('sourceDevice') == source_device
                              and all(row.get(key) == 0 for key in counters)
                              for row in rows)


def complete_radiofax_stop(rows, expected_lines):
    """Require terminal stop, complete coverage and the expected line count."""
    if not rows or expected_lines <= 0:
        return False
    terminal = rows[-1]
    return (terminal.get('imageComplete') is True
            and terminal.get('imageRadiofaxStopDetected') is True
            and terminal.get('imageLinesPublished') == expected_lines
            and terminal.get('imageCoverage') == 1)


def partial_radiofax_stop(rows, width, received_lines, total_lines):
    """Require an exact partial raster and stop, not just approximate coverage."""
    if not rows or width <= 0 or received_lines <= 0 or total_lines <= received_lines:
        return False
    terminal = rows[-1]
    pixels = width * received_lines
    coverage = terminal.get('imageCoverage')
    return (terminal.get('imageRadiofaxStopDetected') is True
            and terminal.get('imagePartial') is True
            and terminal.get('imageComplete') is False
            and terminal.get('imageWidth') == width
            and terminal.get('imageHeight') == total_lines
            and terminal.get('imageLinesPublished') == received_lines
            and terminal.get('imageCompletedPixels') == pixels
            and terminal.get('imageCoveredComponents') == 3 * pixels
            and isinstance(coverage, (float, int)) and not isinstance(coverage, bool)
            and math.isfinite(coverage) and abs(coverage - received_lines / total_lines) < 1e-9)
