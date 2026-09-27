# Law of Cosines vs Intersection of Circles

![Godot Benchmark and Comparison of Law of Cosines and Intersection of Circles](https://shthub.org/u/benjamin/kinetest_godot.png?raw)

This is a comparison of two inverse kinematic methods for the solving of planar two-segment chains. The first utilizes the law of cosines. The second solves for the intersection of two circles where each radii corresponds to a limb segment length.

First, I built the "kinetest" C cli program to test the performance of each method. For a given limb configuration, solving for the midjoint with the law of cosines took roughly 75 nanoseconds while the intersection of circles took roughly 15 nanoseconds.

Next, I rewrote the test within the Godot game engine and rewrote the methods in GDScript. GDScript introduced significant overhead, with both of the solvers taking 1000+ nanoseconds to solve. While the intersection of circles remained significantly faster, the difference in performance became proportionally smaller due to the additional overhead. 

To attempt to lessen this overhead, I tasked OpenAI's Codex with making my C code callable from Godot via a C GDExtension. With this extension, solving in Godot became comparable to the pure C implementation, with the extension taking about 1.33x the time to solve than my original C code. Below is a comparison of the time to solve a given chain using the law of cosines and the intersection of circles with and without a GDExtension written in C. For these results, a release build and maximal compiler optimizations for were utilized.

### Median time to solve IK chain (nanoseconds)

| circles GDScript | cosines GDScript | circles C extension | cosines C extension |
| ---------------- | ---------------- | ------------------- | ------------------- |
| 1,632            | 1,727            | 21                  | 101                 |


## Building C Test

```bash
cc -o kinetest ik_speed_test.c -lm -O3 -Wall -Wextra
```

## Kinetest Usage

Use the json flag to take advantage of the table.py script to format the output:

```bash
./kinetest --json 40000000 0 1 3 2 5 4 | python3 table.py
```

## Godot

The Godot 4.7 workspace compares GDScript and native C solvers with live timing
graphs. See [`godot/`](godot/README.md) for details. On Linux x86_64:

```bash
godot/native/build.sh
godot4 --editor --path godot
```

Restart an already-open editor after building the native extension.

