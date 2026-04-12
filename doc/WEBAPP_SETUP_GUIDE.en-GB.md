# Web App Setup Guide (Novice) - English (UK)

This guide explains, step-by-step, how to enable and use the Decodium remote web dashboard from phone/tablet/PC on your local network.

## 1) What you need

- Decodium `v1.4.3` running on your station computer.
- Station computer and phone/tablet connected to the same LAN/Wi-Fi.
- Decodium main app working normally first (CAT/audio/decode).

## 2) Enable Remote Web Dashboard in Decodium

1. Open Decodium.
2. Go to `Settings` -> `General`.
3. Find section: `ATTENTION: EXPERIMENTAL SECTION` / `Remote Web Dashboard (LAN)`.
4. Enable: `Enable remote web dashboard`.
5. Set fields:
   - `HTTP port`: default `19091` (recommended).
   - `WS bind address`: use `0.0.0.0` for LAN access.
   - `Username`: leave `admin` (or set your own).
   - `Access token (password)`: set a strong password.
6. Press `OK`.
7. Restart Decodium (recommended whenever these fields change).

## 3) Find the station computer IP address

Use the station computer terminal:

- macOS/Linux:
```bash
ipconfig getifaddr en0
```
(if empty, try `en1`)

Example result: `192.168.1.4`

## 4) Open the dashboard from another device

From phone/tablet browser, open:

```text
http://<station-ip>:19091
```

Example:

```text
http://192.168.1.4:19091
```

Login with:

- Username: the value configured in Decodium (default `admin`)
- Password: your `Access token`

## 5) Add to home screen (PWA)

- iPhone/iPad (Safari): Share -> `Add to Home Screen`
- Android (Chrome): menu -> `Install app` or `Add to Home Screen`

After installation you can launch it like a normal app icon.

## 6) What you can do in the web app

- See live station state: mode, band, dial, RX/TX frequencies, TX/monitor state.
- Change mode and band.
- Set RX frequency.
- Enable/disable TX.
- Enable/disable Auto-CQ.
- Trigger slot-aware remote caller action.
- View RX/TX activity table (pause/clear controls).
- Optional experimental waterfall view.

## 7) Security recommendations (important)

- Keep this service on LAN only.
- Use a strong password token.
- Do not expose dashboard port on internet/router unless you know exactly what you are doing.
- If possible, keep firewall rules limited to trusted local devices.

## 8) Quick troubleshooting

### Dashboard does not open

- Check Decodium is running.
- Check `Enable remote web dashboard` is active.
- Check IP/port are correct.
- Check `WS bind address` is `0.0.0.0` for LAN usage.
- Restart Decodium after changing remote settings.

### Login fails

- Re-check username/password in Decodium settings.
- Avoid trailing spaces in username/token.
- Save settings and restart Decodium.

### Opens from same PC but not from phone

- Devices may be on different VLAN/guest network.
- Local firewall may block port `19091`.
- Verify station IP did not change (DHCP).

### Web app looks connected but commands do not apply

- Verify Decodium is in active monitoring state.
- Verify CAT control is available in main app.
- If mode is not FT2, FT2-only controls may be hidden/limited.

## 9) API and advanced endpoints (optional)

- Health:
```text
GET /api/v1/health
```
- Commands:
```text
POST /api/v1/commands
```

These endpoints use the same auth configured for dashboard access.
