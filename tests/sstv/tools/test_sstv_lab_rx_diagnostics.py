#!/usr/bin/env python3
"""A/B and failure controls for laboratory RX diagnostics."""
import json

from sstv_lab_rx_diagnostics import clean_hamdrm_rx_diagnostics, complete_radiofax_stop, partial_radiofax_stop

marker = '[LAB][SSTV] RX diagnostics '
counters = ('discontinuities', 'droppedChunks', 'hamdrmRejectedChunks',
            'hamdrmQueueOverflows', 'processingFailures')
for variant in ('a', 'b'):
    clean = {'sourceDevice': 'BlackHole 2ch', **dict.fromkeys(counters, 0),
             'testVariant': variant}
    line = marker + json.dumps(clean)
    assert clean_hamdrm_rx_diagnostics(line)
    assert not clean_hamdrm_rx_diagnostics('unrelated output')
    for key in counters:
        damaged = dict(clean, **{key: 1})
        assert not clean_hamdrm_rx_diagnostics(line + '\n' + marker + json.dumps(damaged))
        missing = dict(clean)
        del missing[key]
        assert not clean_hamdrm_rx_diagnostics(marker + json.dumps(missing))
    assert not clean_hamdrm_rx_diagnostics(marker + json.dumps(dict(clean, sourceDevice='other')))
    terminal = {'imageComplete': True, 'imageRadiofaxStopDetected': True,
                'imageLinesPublished': 120, 'imageCoverage': 1}
    assert complete_radiofax_stop([terminal], 120)
    assert not complete_radiofax_stop([], 120)
    for key, value in (('imageComplete', False), ('imageRadiofaxStopDetected', False),
                       ('imageLinesPublished', 119), ('imageCoverage', 0.99)):
        assert not complete_radiofax_stop([dict(terminal, **{key: value})], 120)
        missing = dict(terminal)
        del missing[key]
        assert not complete_radiofax_stop([missing], 120)
    assert not complete_radiofax_stop([terminal, dict(terminal, imageRadiofaxStopDetected=False)], 120)
    partial = {'imageComplete': False, 'imagePartial': True, 'imageRadiofaxStopDetected': True,
               'imageWidth': 128, 'imageHeight': 150, 'imageLinesPublished': 120,
               'imageCompletedPixels': 15360, 'imageCoveredComponents': 46080, 'imageCoverage': 0.8}
    assert partial_radiofax_stop([partial], 128, 120, 150)
    for key, value in (('imageComplete', True), ('imagePartial', False), ('imageRadiofaxStopDetected', False),
                       ('imageLinesPublished', 119), ('imageCompletedPixels', 15359),
                       ('imageCoveredComponents', 46079), ('imageCoverage', 0.799)):
        assert not partial_radiofax_stop([dict(partial, **{key: value})], 128, 120, 150)
        missing = dict(partial)
        del missing[key]
        assert not partial_radiofax_stop([missing], 128, 120, 150)
print('PASS RX diagnostics A/B and negative controls')
