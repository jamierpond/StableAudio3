# StableAudio3

Stable Audio 3 text-to-audio inference on
[eacp](https://github.com/eyalamirmusic/eacp)'s GPU/ML stack.

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release -DEACP_UNITY_BUILD=OFF
cmake --build build
build/StableAudio3 --model small --prompt "lofi house loop" \
      --seconds 8 --seed 42 --output out.wav
```

Flags: `--model small|medium` (default `small`), `--prompt`, `--seconds`,
`--secondsTotal` (the length the model is told, its `seconds_total`
conditioning; defaults to `--seconds`), `--seed` (a number, or `random`, which
prints the one it picked), `--samplerSteps` (default 8), `--output`, and
`--fetch-only`, which fetches the model's checkpoints and exits. More `--samplerSteps` trades
time for quality; the medium model at 50 steps makes 30 s in about 10 s on an
M5 Max.

## eacp

eacp is fetched by CPM at configure time (`CMake/Findeacp.cmake`), from the
`jp/stable-audio-infrence` branch of `jamierpond/eacp` until that work merges
upstream. To build against a local eacp checkout instead, pass
`-DCPM_eacp_SOURCE=$HOME/eacp`:

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release -DEACP_UNITY_BUILD=OFF \
      -DCPM_eacp_SOURCE=$HOME/eacp
```

Use `$HOME`, not `~`: CMake does not expand `~`, and a quoted `~` silently
configures against a path that does not exist.

## Checkpoints

Fetched from Hugging Face on first run through `OnlineResource`, at pinned
commits (`Checkpoints.h`):

| `--model` | Repo | Revision | Size |
|---|---|---|---|
| `small` | `stabilityai/stable-audio-3-small-music` | `0fef1392…` | ~3.2 GB |
| `medium` | `stabilityai/stable-audio-3-medium` | `27b5a21b…` | ~9.7 GB |

From each: `model.safetensors`, `t5gemma-b-b-ul2/model.safetensors`,
`t5gemma-b-b-ul2/tokenizer.json`. The codec tests also read
`stabilityai/SAME-L` (`41acf79d…`, `model.safetensors`, ~3.4 GB, not gated),
which the app never fetches.

Both model repos are gated. Accept the terms on the repo page
(`https://huggingface.co/<repo>`) and provide a read token; it is read from
`HF_TOKEN`, then `$HF_HOME/token`, then `~/.cache/huggingface/token` (what
`huggingface-cli login` writes). Without one the fetch fails with HTTP 401.

Files land at
`~/Library/Application Support/StableAudio3/Resources/huggingface/<owner>--<name>/<revision>/<path in repo>`
(`SA3Checkpoints::directory(repo)`), each with a `.resource.json` sidecar.
A pinned commit never changes, so once a file is there the network is not
touched again.

## iOS

`iOS/` is a small app, `StableAudio3iOS`, that runs the small model on the
phone: type a prompt, tap Generate, and it samples 8 s at 8 steps, plays the
result and draws its waveform. While the model loads or works, the space the
waveform takes shows a loader drawn from the forward pass itself (below).

**Expert** under the prompt opens every knob the model has: sampler steps
(1-50), length (1-30 s), the length the model is told (its `seconds_total`
conditioning, 1-60 s, or the same as the length), and the seed, fixed or
random. The line under the status shows what the last generation used. There
is no guidance scale or negative prompt: the model is distilled and the
ping-pong sampler makes one conditioned DiT pass per step. Its schedule is a
fixed linear one from 1 to 0, with no `seconds_start` input. Only the small
model ships, so there is no model choice.

`SA3_AUTORUN` drives it without taps, as the simulator needs: it opens the
panel with the given values and generates once the model is loaded.

```bash
SIMCTL_CHILD_SA3_AUTORUN="steps=4 seed=7 seconds=6 told=4" \
      xcrun simctl launch --console-pty booted ai.tamber.stableaudio3
```

Keys are `steps`, `seed`, `seconds`, `told` and `random=1`. It is built only when
configuring for iOS; the CI flags are eacp's:

```bash
cmake -G Xcode -B build-ios-sim -DCMAKE_SYSTEM_NAME=iOS \
      -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=arm64 \
      -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO -DEACP_UNITY_BUILD=OFF
cmake --build build-ios-sim --config Release --target StableAudio3iOS \
      -- -sdk iphonesimulator

xcrun simctl boot "iPhone 17 Pro"
xcrun simctl install booted \
      build-ios-sim/iOS/Release-iphonesimulator/StableAudio3iOS.app
xcrun simctl launch --console-pty booted ai.tamber.stableaudio3
```

The checkpoint ships inside the bundle, so the app is 3.2 GB. The build copies
the three files from this Mac's download cache into
`StableAudio3iOS.app/huggingface/stabilityai--stable-audio-3-small-music/<revision>/`,
the same layout as the cache, and `SA3Checkpoints::fetch` takes a file from the
app's resources before it fetches anything. Configure fails, naming the path,
until the cache has them: run `build/StableAudio3 --model small --fetch-only`
first, or point `SA3_SMALL_CHECKPOINT_DIR` at a copy. The copy is `cp -c`, an
APFS clone, so it costs no time or disk. Never commit the weights.

### The loader

The loader is Shadertoy's "Octgrams" (`iOS/Reference/octgrams.glsl`, kept
verbatim) ported to eacp's shader EDSL (`OctgramsShader` in `iOS/Backdrop.cpp`)
and rendered at half the screen's resolution, then stretched with linear
filtering into a rounded rectangle. With `drive` at 0 it is the original;
`SA3_OCTGRAMS_CHECK=<iTime>` writes one frame of it to the app's cache
directory, and `iOS/Reference/octgrams.py` (a numpy transcription of the GLSL)
renders the same frame for comparison. At 543x702 and `iTime` 10, 99.8% of
pixels match within 2/255 and the worst is 26/255 on 0.01% of them, where
float rounding over 99 march steps lands on a different side of a box edge.

The port's adaptations, none of which changes a value: GLSL's in-place swizzle
writes (`pos.y += ...`, `pos.xy *= rot(...)`) rebuild the vector, since an EDSL
value has no place to write into; the `for (int i ...)` loop is `loop()` over an
int `var` incremented at the end of the body; `gTime` is a `var` reassigned in
the loop as in the original; `mat2` is `float2x2` from the same columns, with
the same row-vector product; `map()`'s and `box_set()`'s unused `iTime`
argument is kept; the three dead statements at the end of `box()` are kept and
dropped by the emitter, as a GLSL compiler drops them; `iResolution` is the
loader texture's size and `fragCoord` is `uv * iResolution`. The alpha is
written as the original writes it and ignored, as Shadertoy ignores it.

The model comes in through `ForwardProbe`, an `SA3Sampler::StepProbe`. After
each transformer block it records two small reductions into the step's own
pass (`MeanSquareColumnsKernel`, `MeanOfRunsKernel`): one float per block, the
hidden state's mean square, into a buffer the main thread reads while the GPU
writes it, so the loader sees blocks finish inside a step rather than after
it. After each step it reduces the model's clean-latent estimate and the next
latent to their per-channel mean squares, 512 floats read back once the step
has finished. Sampling 8 s at 8 steps in the simulator takes 0.46-0.47 s
without the probe and 0.48-0.57 s with it.

| Shader input | Model quantity |
|---|---|
| `drive` | 1 while a generation has sampler data, easing to 0 otherwise; 0 is the bare original |
| march step `i`'s weight on `ac` | the activation of transformer block `floor(i * depth / 99)`: the hidden state's mean square after that block, log-scaled between the step's quietest and loudest block. Blocks this step has reached are at full weight, the rest at 0.45 of the previous step's, so a bright front sweeps through the march as the DiT works down its layers |
| `iTime` speed | the activation of the last block finished: from 0.35x to 3.35x wall time |
| `p` warp and the `ac` tint | the per-channel energy of the clean-latent estimate (256 channels), sampled by the angle of `p`: radial warp of up to ±18% and a blue-to-amber tint per direction |
| box scale (`1.5` in `2. - abs(sin(gTime * .4)) * 1.5`) | the RMS of the latent the next step starts from: `1 + 1.5 * (rms - 1)`, clamped to 0.5-1.1 |
| base colour | the timestep: the original's blue base fades to amber as the timestep goes from 1 to 0 |

Each term is `mix(original, model term, drive)`. Different prompts and seeds
differ visibly at the same step, mostly through the channel energies.

The app prints the process's `phys_footprint` (what jetsam judges) after each
stage and shows the latest under the status line.

## Tests

The SA3 test targets (`SA3Codec*`, `SA3DiT*`, `SA3Sampler*`,
`SA3TextEncoder*`) read checkpoints from the same place and never download.
Run the app once per model first; for the `SA3CodecSameL*` tests, put SAME-L's
`model.safetensors` in `SA3Checkpoints::directory(SA3Checkpoints::sameL)`.

They are registered with ctest (`ctest --test-dir build -R SA3`). A case whose
checkpoint is not cached prints `skipped: checkpoint not cached (<path>)` and
passes; the suites that need no
checkpoint (`SA3CodecFastTests`, `SA3CodecPatchedPretransformGoldenTests`,
`SA3DiTUnitTests`, `SA3SamplerFastTests`) run everywhere.
`EACP_REQUIRE_CHECKPOINTS=1` turns every skip into a failure, for a machine
that is meant to have them.

### In CI

`.github/workflows/sa3-goldens.yml` is the one lane that has checkpoints. On a
macOS runner it builds the app and its suites, fetches the small model's three
files (`StableAudio3 --model small --fetch-only`, the app's own fetch) into an
`actions/cache` entry keyed on `Checkpoints.h`, generates four seconds as a
check that the runner has a Metal device (the WAV is not compared: the
bit-exactness in `PERFORMANCE.md` is between commits on one machine, not across
GPUs), and runs every `SA3` case but the medium and SAME-L ones with
`EACP_REQUIRE_CHECKPOINTS=1`. To turn it on, a maintainer accepts the terms on
`stabilityai/stable-audio-3-small-music` with a Hugging Face account and adds
that account's read token as the repository secret `HF_TOKEN` (Settings →
Secrets and variables → Actions). Without the secret, as on a fork, the job is
skipped.
