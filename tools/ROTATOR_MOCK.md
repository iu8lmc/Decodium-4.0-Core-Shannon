# Local rotator mock

`rotator_mock.py` is a dependency-free, asynchronous local rotator used to
exercise Decodium's satellite rotator controls without a physical device.
It logs every command, updates a simulated azimuth/elevation, and returns
feedback where the selected protocol supports it.

Run one protocol at a time using the same host and port configured in
Decodium:

```sh
python3 tools/rotator_mock.py --protocol pstrotator
python3 tools/rotator_mock.py --protocol catrotator
python3 tools/rotator_mock.py --protocol hamlib
```

Defaults:

| Protocol | Command endpoint | Feedback |
| --- | --- | --- |
| PSTRotator | UDP `127.0.0.1:12000` | UDP `127.0.0.1:12001` |
| CatRotator | UDP `127.0.0.1:12000` | Not available |
| Hamlib rotctld | TCP `127.0.0.1:4533` | Same TCP connection |

In Decodium, open Satellite tracking, select the matching protocol, set host
to `127.0.0.1`, enable the rotator, then use the manual controls:

- left/right: azimuth `-10°/+10°`;
- up/down: elevation `+5°/-5°`;
- Rotor STOP and Rotor PARK exercise the stop/park commands.

For PSTRotator the mock answers `AZ?` and `EL?` on the configured feedback
port. For Hamlib it accepts `P az el`, `p`, `S`, and `K` using the asynchronous
TCP line protocol. CatRotator is intentionally feedback-free, so the UI should
show that no feedback is available while still logging all movement commands.

To run multiple mock instances, choose separate ports, for example:

```sh
python3 tools/rotator_mock.py --protocol pstrotator --port 12000 --feedback-port 12001
python3 tools/rotator_mock.py --protocol catrotator --port 12002
python3 tools/rotator_mock.py --protocol hamlib --port 4533
```
