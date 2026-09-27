# Build optimization comparison

Godot 4.7.2.stable.official.ed1daf0bf; cc (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0.

Median of 7 runs per configuration; each run averages 1,200 measured updates
after 100 warmup updates. Headless, fixed reachable target (80,60), lengths 100/100.
All numbers are nanoseconds per solve. Same C source and same GDScript source in each build.

| Godot runtime | C optimization | Script circles | Script cosines | C circles | C cosines |
|---|---|---:|---:|---:|---:|
| editor | -O0 | 2092.7 | 2233.1 | 58.4 | 139.5 |
| debug | -O0 | 2065.4 | 2199.4 | 59.8 | 142.8 |
| release | -O0 | 1571.4 | 1665.0 | 57.3 | 137.0 |
| editor | -O3 | 1986.7 | 2086.7 | 19.2 | 91.7 |
| debug | -O3 | 1986.7 | 2118.9 | 20.3 | 96.3 |
| release | -O3 | 1631.6 | 1727.0 | 21.0 | 100.6 |

Native timings exclude Godot binding/result-allocation overhead. GDScript timings include
its script loop and result indexing. C uses float32; GDScript scalar math uses float64.
The C loop and solver translation units are separate, with LTO disabled at both optimization levels.
Calls into the system math library still use that library's optimized implementation at -O0.
Run order is shuffled each round. Timings are machine/load dependent; raw runs and build logs
are in `godot/builds/optimization-study/`. The normal project library is unchanged.

Reproduce with `python3 godot/native/compare_builds.py --templates /path/to/templates`.
