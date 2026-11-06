#!/usr/bin/env python3
"""Offline controls for the isolated sharing provider preflight."""
from pathlib import Path
from run_share_app_transfer_lab import check_loopback_provider

certificate = Path(__file__).parent.parent / 'fixtures' / 'localhost-test-only.crt'


def trial(body, status=200, fail=False):
    calls = []

    class Response:
        def __init__(self):
            self.status = status

        def read(self, limit):
            calls.append(('limit', limit))
            return body

    class Connection:
        def __init__(self, host, port, timeout, context):
            calls.append(('connect', host, port, timeout, context.check_hostname))

        def request(self, method, path):
            calls.append(('request', method, path))
            if fail:
                raise TimeoutError('simulated loopback timeout')

        def getresponse(self):
            return Response()

        def close(self):
            calls.append(('close',))

    try:
        check_loopback_provider('https://127.0.0.1:12345/', certificate, Connection)
        outcome = 'ok'
    except RuntimeError:
        outcome = 'rejected'
    assert calls[-1] == ('close',)
    assert ('request', 'GET', '/api/v1/capabilities') in calls
    return outcome


for suffix in (b'A', b'B'):
    assert trial(b'{"endToEndEncryptionEnvelope":true,"extra":"' + suffix + b'"}') == 'ok'
    assert trial(b'{"endToEndEncryptionEnvelope":false,"extra":"' + suffix + b'"}') == 'rejected'
    assert trial(b'not-json-' + suffix) == 'rejected'
    assert trial(b'x' * 65537) == 'rejected'
    assert trial(b'{}', status=503) == 'rejected'
    assert trial(b'', fail=True) == 'rejected'
print('PASS sharing HTTPS preflight A/B and failure controls')
