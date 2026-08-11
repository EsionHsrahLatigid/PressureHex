# PressureHex

PressureHex is a YUP stereo compressor designed as an aggressive control surface for Digital Harsh Noise. A feed-forward peak/RMS detector, log-domain soft-knee gain computer, independent attack and release, and optional lookahead can either pin transient walls or make them breathe in severe blocks. Hosted builds preserve silence; Standalone adds an audition source and meters only at compile time.

## Identity and formats

- App/plugin ID: `jp.ehl.pressurehex`
- Vendor: `ehl_`; AU manufacturer: `EHL1`; AU subtype: `PrHx`
- Version: `0.1.0`
- macOS: Standalone, VST3, AUv2
- Windows: Standalone, VST3
- Stereo effect, no MIDI

## Parameters

- `Threshold`: `-60` to `0` dB gain-reduction onset.
- `Ratio`: `1:1` to `20:1` compression slope.
- `Attack`: 0.1–100 ms gain-reduction attack.
- `Release`: 5–800 ms recovery.
- `Knee`: 0–30 dB soft-knee width.
- `Focus`: continuous RMS-to-peak detector blend.
- `Lookahead`: 0–20 ms delayed program path.

## Research basis

The survey compared common feed-forward compressor controls and transfer behavior in [FFmpeg's official audio filter documentation](https://ffmpeg.org/ffmpeg-filters.html) with the detector and level-domain context in [ITU-R BS.1770-5](https://www.itu.int/rec/R-REC-BS.1770-5-202311-I/_page.print) and dynamics-processing material in [AES E-Library 174](https://secure.aes.org/forum/pubs/journal/?ID=174). PressureHex is not a loudness meter or a model of one hardware compressor; its peak/RMS blend, soft-knee interpolation, and hard output bound are product choices.

## Build and artifacts

```sh
cmake --preset engine-debug
cmake --build --preset engine-debug --parallel
ctest --preset engine-debug --output-on-failure

cmake --preset plugin-release
cmake --build --preset plugin-release --parallel
ctest --preset plugin-release --output-on-failure
```

Human-facing products are staged under `artifacts/plugin-release/<platform-arch>/` in `standalone/`, `vst3/`, and macOS `au/`. `build/` is internal compiler state.

## CI and safety

Caller workflows pin `EsionHsrahLatigid/yup-actions` to a full commit SHA. CI tests and packages macOS arm64 and Windows x64, producing checksummed latest ZIPs; `v*` tags promote exact-SHA CI artifacts without rebuilding. The audio callback allocates no memory and performs no locks, I/O, logging, or UI work. Parameters, non-finite input, gain, and output are bounded; compression response, detector behavior, determinism, extremes, hosted silence/state, and Standalone audition are tested.
