# RTL-SDR dependency notice

Decodium optionally includes the RTL-SDR receive-input backend and links to
`librtlsdr` when that feature is enabled.

Release packages build `librtlsdr` from the RTL-SDR Blog maintained source
release `v2.0.2`:

- Source: <https://github.com/steve-m/librtlsdr>
- License: GNU General Public License, version 2 or any later version
  (GPL-2.0-or-later)

Decodium is distributed under GPL-3.0-or-later.  The dependency's “or later”
licensing permits distribution of the combined work under GPL-3.0-or-later.
The complete corresponding source for the RTL-SDR component is available from
the source link above; release builds use the tagged source archive unchanged
except for normal compiler and installation settings.

The RTL-SDR backend in Decodium is receive-only.  It never enables PTT or RF
transmission.
