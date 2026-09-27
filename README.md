# Kinetest

## Godot

A script-free Godot 4.7 project is in [`godot/`](godot/README.md).
Open `godot/project.godot`, or run `godot4 --editor --path godot`.

## Build

```bash
cc -o kinetest ik_speed_test.c -lm -O3 -Wall -Wextra
```

## Usage

Use the json flag to take advantage of the table.py script to format the output:

```bash
./kinetest --json 40000000 0 1 3 2 5 4 | python3 table.py
```
