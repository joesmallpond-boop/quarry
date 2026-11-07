# QUARRY by kalabash: four-track granular workstation (VST3 / AU / Standalone)

Four parallel stereo tracks. Each one runs a fixed five-stage chain:

```
MATERIAL → GRANULAR → FILTER → COLOR → SPACE → mixer → glue comp → DJ filter → punch FX → out
```

Two plugins are built from the same source:

| Plugin | Type | Use it for |
|---|---|---|
| **QUARRY** | Instrument (VST3 / AU `aumu`) | Playing samples from MIDI; record live audio through its input / sidechain |
| **QUARRY FX** | Audio effect (VST3 / AU `aumf`) | Dropping on an audio track to loop, granulate and smear what's playing |

## The stages

- **Material**: *Tape* (32 s live loop with overdub, varispeed −2…+2× including reverse, wow/flutter), *Disc* (sample playback, free-running or MIDI-triggered), *Poly* (8-voice sampler with key tracking). Start/length set the loop region.
- **Granular**: a 4-second rolling buffer with up to 128 grains. Controls: position, size, density, spray, scan, pitch + random pitch (with semis/fifths/octave quantise), window shape, stereo spread, reverse probability, freeze.
- **Filter**: a state-variable filter sweeping LP → BP → HP, morphing into a resonator bank of up to 48 bands. The bank's root note, harmonic spread, decay and tilt are adjustable, with optional key tracking.
- **Color**: two-band drive (Soft / Hard / Fold / Tube) with a low/high balance, tilt EQ, bit-depth reduction, downsampling, a noise injector and a glue compressor.
- **Space**: stereo delay (free or tempo-synced) with pitch-shifted shimmer feedback and freeze, feeding an 8-line FDN reverb.
- **Mod**: 4 modulators per track (Sine, Tri, Saw, Square, S&H, Random Walk, Envelope triggered by notes, Envelope Follower). Each one can target any of 53 track parameters.
- **Perform**: 8 macros, each with two targets on any track and bipolar depth. There are also 4 punch-in FX: Stutter, Tape Stop, Sweep and Freeze.
- **Scenes**: 128 snapshot slots (8 banks × 16), with optional glide between scenes. MIDI Program Change *N* recalls scene *N+1*.
- **Routing**: any track's input can be *Ext In* or the output of any track, itself included. That makes feedback networks and serial chains possible.

## Panel

| Control | Function |
|---|---|
| big knob | master volume |
| project | track overview · soft keys: load / rec / init / clear |
| config | MIDI channel, input, routing per track; global settings |
| 8 encoders | the 8 parameters shown on screen (drag; double-click = default). The screen bars can also be dragged. |
| 5 device keys | Material, Granular, Filter, Color, Space. Press again for the next page |
| mod + △□⬡○ | soft keys pick pages; in **mod** they pick modulator 1–4 |
| ▷□ (play) | start/stop playback · **ctrl + play** arms recording on the current track (switches it to Tape) |
| copy | copy current track, then pick the track to paste into |
| clear | clear tape + unload sample · **ctrl + clear** resets the whole track |
| ctrl | modifier · **ctrl + device key** resets that device |
| track | choose track 1–4 (or click the LEDs at the top) |
| scene / mix / perform | scenes, mixer (level/pan, mute/solo, master), macros |
| temp | latches the soft keys into hold-to-punch FX: stutter / tape stop / sweep / freeze |

Keyboard: `1-4` select track, `space` play, `R` record, `C` ctrl, `T` temp. Drop audio files onto the panel to load them into the current track; several files fill consecutive tracks.

## Building

**Via GitHub (recommended):** every push to `main` runs `.github/workflows/build.yml`:
- **macOS**: `QUARRY-<version>-mac.dmg` contains **`QUARRY Installer.pkg`**. That one installer puts in the standalone app (`/Applications`), the VST3 plug-ins and the AU plug-ins (both QUARRY and QUARRY FX), all universal for Apple Silicon and Intel. *Customize* in the installer lets you pick which parts to install.
- **Windows**: `QUARRY-windows.zip` with the VST3 plug-ins and the standalone .exe.

Download them from the run's **Artifacts** section on the Actions tab. Pushing a tag such as `v1.0.0` also attaches them to a GitHub Release.

The installer is unsigned by default. To sign and notarise it, add these repo secrets: `MACOS_CERTIFICATE_P12` (base64 .p12 holding both Developer ID certificates), `MACOS_CERTIFICATE_PASSWORD`, `MACOS_APP_SIGN_ID`, `MACOS_INSTALLER_SIGN_ID`, `APPLE_ID`, `APPLE_APP_PASSWORD` and `APPLE_TEAM_ID`. The workflow then signs and notarises automatically.

**Locally:**
```bash
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release
```
JUCE 8.0.4 is fetched automatically. You can also drop a JUCE checkout into `./JUCE`.

**Tests:** `cmake -B build-test -DQUARRY_TESTS=ON && cmake --build build-test --target QuarryTests`. The test run renders every stage, checks for NaNs and runaway feedback, measures CPU and writes screenshots of every screen.

## Notes
- Sample file paths are saved with the project, and the sample reloads when the project opens. Tape recordings live in memory only, so bounce anything you want to keep.
- Unsigned installer: on macOS 15+, open it once, then go to System Settings > Privacy & Security > *Open Anyway*. On older macOS, right-click > Open.
- The developer / manufacturer shows as **kalabash** in every host.
