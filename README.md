# Kinetest

## Build

```bash
cc -o kinetest ik_speed_test.c -lm
```

## Usage

Use the json flag to take advantage of the table.py script to format the output:

```bash
./kinetest --json 40000000 0 1 3 2 5 4 | python3 table.py
```
