# Handoff: StableAudio3 and the eacp library PR

State on 2026-10-07. Two repos, two open PRs, one blocker.

| | Repo | Branch | PR | Head |
|---|---|---|---|---|
| Library | `~/eacp` (fork `jamierpond/eacp`, upstream `eyalamirmusic/eacp`) | `jp/stable-audio-infrence` | [eyalamirmusic/eacp#62](https://github.com/eyalamirmusic/eacp/pull/62), base `develop` | `de34af92` |
| App | `~/projects/StableAudio3` (`jamierpond/StableAudio3`) | `jp/eacp-develop-merge` | [jamierpond/StableAudio3#1](https://github.com/jamierpond/StableAudio3/pull/1), base `main` | `b3d34d69` |

`jamierpond/eacp#5` is an old draft that only existed to run CI. Leave it or close it; nothing depends on it.

## Blocker: Windows CI hangs on eacp#62

All four Windows lanes (MSVC, clang-cl, x64 and ARM64) ran 2334 of 2335 tests, then sat on one until the 6 h job limit cancelled them (run 37528684499). Every other lane is green: macOS, iOS sim, Android, Linux GCC, Clang, Clang Vulkan. `develop` itself is green on Windows in ~11 min.

The test that never finished, on all four lanes:

```
Executor/aBodyNamingNoBlockIsInvalid   (Tests/CpuCompute/ExecutorTests.cpp:2683)
```

It comes from develop's `eacp-cpu-compute`. It builds a graph whose `if` names block 7 and whose loop names block -3, which do not exist, and expects the executor to refuse them.

Hypothesis, unverified: our branch made `ShaderGraph` record sequence points, so `addIf`/`addLoop` now read the named block (`Block::opened`) to stamp `Statement::sequence`. With a block index that does not exist that is an out-of-range vector index. On macOS and Linux it is silent UB. In a Windows Debug build the MSVC STL's checked iterators raise a CRT assert, which opens a modal dialog, and a dialog nobody can click looks exactly like a 6 h hang. Clang-cl uses the same STL, which fits all four lanes failing identically.

How to confirm: on `jamie@tamby-windows`, build `CpuComputeTests` Debug from `jp/stable-audio-infrence` and run that one case. A dialog or an abort confirms it. The fix is to bounds-check the block index in `ShaderGraph::addIf`/`addLoop` (`Lib/eacp/GPU/Codegen/ShaderGraph.cpp`) before reading it, leaving the refusal to the executor as develop intended. Add a timeout to the Windows test step so a dialog fails fast rather than burning 6 h.

## What is on eacp#62

Library work only. The app left in `409a50f6` and lives in the StableAudio3 repo.

- `Lib/eacp/ML` tensor and kernel library, target `eacp-ml-kernels`, beside develop's Core ML runner `eacp-ml`. Safetensors loader, `LinearF32` retiled, banded and tiled attention, per-head norms, RoPE over segments, `Tensor::columns()` views.
- `BufferPool` behind `Device::makeBuffer(bytes)`, with `Device::lastSubmission`/`hasFinished`/`memoryBudget`/`perDevice`.
- `compileComputeCached`, `sharedKernel`, `ShaderBinaryCache` on disk.
- `ShaderGraph` sequence points ("a handle is a value") and the emitter sequencing fix.
- `TimingScope::EachDispatch`, `CallCost`.
- Off-Metal SIMD-group product without barriers; D3D12 65535 grid cap documented and warned about.
- Merge of `origin/develop` at `8c01c2d1` (`836c84b3`), resolved three-way in 10 files.
- `409d8dc9` removed the shader golden corpus (159 files under `Tests/GPU/Golden`, `ShaderGoldenTests`, `ShaderGolden.h`, the README section) because it made the PR unreadable.

Behaviour changes existing eacp users will notice, all stated in the PR body:
- `makeBuffer(bytes)` returns recycled memory, not zeros, on Metal.
- `Buffer::update` waits for the newest submission.
- `Device::shared()` asserts main-thread use in Debug.
- The Vulkan pipeline cache moved to `FilePath::appCacheDirectory()`. Android keeps its own folder; that was a judgment call in the merge.

## What is on StableAudio3#1

- CPM pin in `CMake/Findeacp.cmake` moved to `de34af92` (fork, `jp/stable-audio-infrence`). Bump it again whenever eacp#62 gets a new commit, especially the Windows fix.
- `Tests/ShaderGolden.h` moved in from eacp. `CodegenCommon.h` is still taken from `${eacp_SOURCE_DIR}/Tests/GPU`, which exists on develop.
- No app code changed. 77 of 77 tests pass locally with `EACP_REQUIRE_CHECKPOINTS=1`, including 30 shader goldens without a re-bless.
- Small 12 s seed 42 output is byte-identical between this branch and the app built against pre-merge eacp `fba77a1e`.
- `HF_TOKEN` is now set as a repo secret. The SA3 goldens job was skipped before that; a re-run of run 37533768869 was requested on 2026-10-07 and was in progress at handoff. Check it.

Untracked in the repo root, not part of the PR: `first-song.wav`, `west-coast-funk-medium-30s.wav` (medium, 30 s, seed 23, "sun-drenched west coast funk, slap bass, clavinet, tight live drums, horns, 108 bpm"), and the `build*` dirs.

## Audit findings from 2026-09-23, and where they stand

An audit of the perf work found the following. Fixed means verified in the current code.

| Finding | State |
|---|---|
| Bit-exact claim | Verified. HEAD WAVs matched the pre-optimisation ones byte for byte, medium and small. |
| Banded attention window end not clamped to row count | Fixed (`BandedAttention.cpp:33`). |
| Pooled `Buffer` holds a raw pool pointer, use-after-free if it outlives its Device | Fixed, now a `weak_ptr` link (`BufferPool.cpp:33`). |
| Pool never frees storage released after the last submission, so an idle process keeps its working set | Not verified as fixed. Check `BufferPool::give`/`promoteFinished` with an idle test. |
| D3D12 grids past 65535 only log; NVIDIA tolerates X, other drivers may write nothing | Open. Still a warning, not a split. Medium decode's 1-D dispatches are 72k to 132k groups. |
| `sharedKernel` keeps the last caller's buffer pointers; a forgotten member binds a dangling one | Merge note says `bindResources` now throws on an unassigned member. Confirm that covers it. |
| No NaN or silence check on the generated WAV; unknown CLI flags ignored, `--steps` hardcoded to 8 | Open, app side. |
| PERFORMANCE.md headline numbers had no raw runs behind them; PyTorch "cold" wall included a second generate | Docs now say so. A clean `benchmark.py` run on mains is still owed. |
| CI never fetched checkpoints, so model goldens covered nothing | App repo has a goldens job now; depends on `HF_TOKEN`, now set. |

## Environment gotchas

- **This shell runs inside cowterm, an eacp app that exports `EACP_ROOT_LOOP=1`.** Every eacp test that asks whether an event loop is running sees the host's. `DynamicLibrary/unloadWithNoLoop` fails for that reason alone. Run tests with `env -u EACP_ROOT_LOOP ctest ...`.
- The laptop was on battery for much of this; every timing taken then is tainted. `Benchmark/benchmark.py` refuses to run on battery.
- Checkpoints are cached at `~/Library/Application Support/StableAudio3/Resources/huggingface`. The SAME-L checkpoint for the codec tests is fetched by hand.
- Windows box for reproducing the hang: `jamie@tamby-windows`, git-bash over ssh.

## Resume order

1. Confirm and fix the `Executor/aBodyNamingNoBlockIsInvalid` hang on Windows, push to `jp/stable-audio-infrence`, re-run eacp#62 CI.
2. Bump the StableAudio3 pin to that sha; check the goldens job runs green with `HF_TOKEN`.
3. Decide on the open audit items above: pool idle release and the D3D12 grid split are the two that matter for a plugin or server.
4. Re-run `benchmark.py` on mains and replace the PERFORMANCE.md tables with numbers that have JSON behind them.
