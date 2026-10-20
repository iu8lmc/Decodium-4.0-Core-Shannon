# Decodium 4 v1.0.673

This maintenance release adds two operator controls:

- CW supports optional transmit while typing. Enable **TX while typing** in
  the CW window; newly entered text is queued and sent after a short pause.
- SSB radio controls include **ATU Tune** and **Power off radio** for Hamlib
  CAT connections. Power off requires confirmation. Successful power-off
  disconnects the CAT session. Both controls are unavailable while transmitting.

Hamlib reports unsupported tuner or power commands in the SSB status area.
Hardware behavior depends on the radio model and its Hamlib driver. The Windows
installer and portable bundle are built from the v1.0.673 source state.
