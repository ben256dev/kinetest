#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
if [[ "$(uname -s)" != Linux || "$(uname -m)" != x86_64 ]]; then
    echo 'This build script currently targets Linux x86_64.' >&2
    exit 1
fi
optimization="${OPT_LEVEL:-3}"
case "$optimization" in
    0|1|2|3|g|s) ;;
    *) echo 'OPT_LEVEL must be 0, 1, 2, 3, g, or s.' >&2; exit 1 ;;
esac
mkdir -p generated bin
(cd generated && "${GODOT:-godot4}" --headless --dump-gdextension-interface --dump-extension-api)
python3 - <<'PY'
import json
from pathlib import Path
api = json.loads(Path('generated/extension_api.json').read_text())
sizes = next(x['sizes'] for x in api['builtin_class_sizes'] if x['build_configuration'] == 'float_64')
sizes = {x['name']: x['size'] for x in sizes}
Path('generated/abi_sizes.h').write_text(
    f'#define IK_VARIANT_SIZE {sizes["Variant"]}\n'
    f'#define IK_STRING_SIZE {sizes["String"]}\n'
    f'#define IK_STRING_NAME_SIZE {sizes["StringName"]}\n')
PY
# Separate translation units and no LTO prevent hoisting repeated solver calls.
"${CC:-cc}" -std=c11 "-O$optimization" -g -Wall -Wextra -Werror -fPIC -fvisibility=hidden -fno-lto -c solvers.c -o generated/solvers.o
"${CC:-cc}" -std=c11 "-O$optimization" -g -Wall -Wextra -Werror -fPIC -fvisibility=hidden -fno-lto -Igenerated -c extension.c -o generated/extension.o
"${CC:-cc}" -shared -fno-lto generated/solvers.o generated/extension.o -lm -o generated/libkinetest.so
# Replace atomically so an already-running editor can keep its loaded library.
mv generated/libkinetest.so bin/libkinetest.so
printf 'Built %s/bin/libkinetest.so (-O%s)\n' "$PWD" "$optimization"
