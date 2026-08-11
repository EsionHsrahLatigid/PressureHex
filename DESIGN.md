# Design

## Source of truth
- Status: Active
- Last refreshed: 2026-08-12
- Primary surfaces: YUP Standalone, VST3, AUv2 editor
- Evidence: dynamics references and the nine-effect Digital Harsh Noise UI survey

## Product
- Goal: make density and transient violence directly playable while remaining bounded and automatable.
- Non-goals: mastering transparency, loudness measurement, MIDI instrument behavior.
- Signal path: stereo detector -> log-domain soft-knee gain computer -> smoothed gain -> lookahead program delay -> bounded output.
- Main controls: Threshold, Ratio, Attack, Release, Knee, Focus, Lookahead.

## Unified visual system
- 960x540 resizable canvas with preserved aspect ratio.
- Seven-column single-row parameter grid with textual values and native host gestures.
- Black/white/gray only; square `fillRect` geometry, scanlines, grid bars, no gradients, glow, rounded cards, or flashing.
- Standalone-only audition buttons and 32-step input/output meters; hosted editors expose no generator controls.

## Interaction and accessibility
- High-contrast text and values remain visible at all times.
- Meter motion is functional and limited to a 30 Hz decaying display.
- Hosted silence stays silent; Standalone audition is runtime-only and never serialized.

## Implementation contract
- C++20/YUP; no new runtime dependency or external asset.
- Audio thread performs no allocation, locks, I/O, logging, or UI calls.
- Seven stable parameter IDs; state magic `PHX1`.
- App/plugin ID `jp.ehl.pressurehex`; vendor `ehl_`; AU `PrHx` / `EHL1`.
- Tests cover compression response, Focus behavior, determinism, extreme values, hosted silence/state, and Standalone audition/meter isolation.

## Open questions
- [ ] Tune the most violent fast-attack presets after multi-host listening tests.
