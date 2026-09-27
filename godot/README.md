# Godot IK workspace

The scene runs both two-link IK solvers in GDScript and optimized native C.
Only the selected backend/solver result drives the limb. Native C is the default;
use the limb inspector's `Use Native Result` and `Solver` properties to choose.
Move the mouse within reach to compare the full algorithms. Outside reach,
both solvers return early; the UI identifies that condition.

## Build and run (Linux x86_64)

Requires Godot 4.7, a C compiler, Python 3, and Bash. From the repository root:

```bash
godot/native/build.sh
godot4 --editor --path godot
```

Restart an already-open Godot editor after building the extension, then press F5.
After native source changes, rebuild and restart Godot. The build uses `-O3` for
both debug/editor and release, without fast-math or LTO. Set `GODOT` or `CC` to
choose other executable paths. Generated Godot headers, ABI sizes, and binaries
are ignored by Git; rebuild on a fresh checkout. The current build script and
extension manifest support Linux x86_64 only.

The extension uses Godot's direct C GDExtension API: no engine rebuild or C++
bindings are needed. The engine header and opaque value sizes are generated
from the installed Godot executable. See the
[official C example](https://docs.godotengine.org/en/4.6/tutorials/scripting/gdextension/gdextension_c_example.html).

## What the measurements mean

- GDScript: 100 calls per solver per frame, timed using `Time.get_ticks_usec()`.
  Values are microseconds per call and include script loop/indexing overhead.
- Native C: 4,096 calls per solver per frame, timed inside the extension using
  `clock_gettime(CLOCK_MONOTONIC)`. Values are nanoseconds per call and include
  the C loop, function call, reachability checks, and solver math. Marshaling,
  warmup, and result construction are outside this timer.
- Both solver orders alternate each frame. C performs 16 warmup calls per solver.
  Solvers are compiled separately from the benchmark loop without LTO so the
  compiler cannot hoist or eliminate repeated calls. Both consume identical
  targets/lengths/orientations; the C implementation uses `float`/`sqrtf`/trig-f
  like the original C project, while GDScript scalar math uses its native floats.
  This measures the actual implementations, not an isolated language overhead.
- `Full C batch + bridge` is the complete Godot-to-C round trip for **both batches**,
  including warmup and result creation. It is not the latency of one solve.
- The speedup compares the rolling mean per-call timings, not whole-frame speed.
  The app deliberately runs all four implementations to compare them.
- Two graphs retain the latest 240 frames in circular buffers, each with its own
  scale (GDScript microseconds, native nanoseconds). Cyan is circles, orange is
  cosines. Histories can mix reachable and unreachable inputs as the mouse moves.
- `Pose difference` compares the selected solver's native and script midjoints
  on reachable targets. Small float-rounding differences are expected.

## Verification and repeatable comparison

```bash
godot4 --headless --path godot --editor --quit
godot4 --headless --path godot --script res://native/test_native.gd
```

The test checks both C binding paths, 500 reachable targets and both orientations,
link lengths, boundary/invalid inputs, selected backend results, and buffer
wraparound. It prints a comparison from the actual scene using a fixed reachable
target `(80, 60)` and link lengths `100/100`. To also render and capture the UI:

```bash
godot4 --path godot --script res://native/test_native.gd -- --capture
```

The screenshot is written to `/tmp/kinetest-native.png`.

## Files

- `native/solvers.c`: pure C solver math.
- `native/extension.c`: Godot registration/bindings and native batch timing.
- `scripts/limb.gd`: script solvers, backend selection, and limb drawing.
- `scripts/performance_ui.gd`: live stats and circular-buffer graphs.
- `scenes/main.tscn`: workspace and CanvasLayer UI.

The original standalone C benchmark remains in the repository root.

## Comparing optimization and export modes

`OPT_LEVEL=0 godot/native/build.sh` builds an unoptimized native extension;
`godot/native/build.sh` restores the default `-O3` build. Restart Godot after either.

For an isolated comparison without changing the main project's library, place
matching Godot Linux export templates (`linux_debug.x86_64` and
`linux_release.x86_64`) in a folder and run:

```bash
python3 godot/native/compare_builds.py --templates /path/to/templates
```

This tests `-O0` and `-O3` in the editor executable and actual debug/release exports.
It runs each configuration seven times in shuffled order, checks build-mode flags,
and reports medians from the identical fixed-target workload. Generated exports
and raw results stay in `godot/builds/optimization-study/`; the readable results
are saved to `native/optimization_results.md`.
