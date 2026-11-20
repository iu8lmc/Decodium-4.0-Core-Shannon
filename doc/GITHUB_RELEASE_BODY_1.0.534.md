# Decodium 4.0 v1.0.534

Version 1.0.534 brings the fork in line with upstream 1.0.533 and translates the
100 interface strings that release left behind.

## Included from upstream

- **1.0.533: Auto Call, CAT4OM, RTL-SDR and map improvements.** Automatic replies
  to eligible CQ stations, work on the CAT4OM backend, SSB listening filters for
  the RTL-SDR path (bandwidth, audio AGC, notch, noise reduction), and map and
  callsign-intelligence work including eQSL, LoTW and QRZ confirmation downloads.
- That release also extends the 3D spectrum added in 1.0.532: the GPU-direct FFT
  keeps its history only in RHI textures, while a stacked-trace surface needs
  CPU-visible dB rows, so the bridge now selects its asynchronous FFT fallback
  whenever 3D is switched on. That is a real limitation of the original
  implementation, found and fixed upstream.

## Translations

- 100 interface strings were missing from the catalogs. 32 already existed in
  other contexts and were reused; **68 were new and are now translated in all
  14 languages**.
- Thirty of those, covering the eQSL, LoTW and QRZ confirmation status messages,
  were written in Italian in the source, so anyone running the program in
  English was reading Italian. They now have proper English along with the other
  twelve languages.
- The rest cover the Auto Call panel, the RTL-SDR SSB listening filters, the ADI
  confirmation import and a handful of map strings.
- Every catalog holds 5014 messages with none left untranslated.
