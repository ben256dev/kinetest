# Kinetest

## Godot

The Godot 4.7 workspace compares GDScript and native C solvers with live timing
graphs. See [`godot/`](godot/README.md) for details. On Linux x86_64:

```bash
godot/native/build.sh
godot4 --editor --path godot
```

Restart an already-open editor after building the native extension.

## Build

```bash
cc -o kinetest ik_speed_test.c -lm -O3 -Wall -Wextra
```

## Usage

Use the json flag to take advantage of the table.py script to format the output:

```bash
./kinetest --json 40000000 0 1 3 2 5 4 | python3 table.py
```
