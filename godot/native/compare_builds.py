#!/usr/bin/env python3
"""Compare identical workloads in editor, debug, and release Godot binaries.

Requires matching Linux export templates. Builds in ignored godot/builds without
changing the extension loaded by the user's main project.
"""
import argparse
import json
import os
from pathlib import Path
import platform
import random
import shutil
import statistics
import subprocess


def run(command, *, env=None, log=None):
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, env=env, timeout=180)
    if log:
        log.write_text(result.stdout)
    if result.returncode or 'SCRIPT ERROR:' in result.stdout or '\nERROR:' in result.stdout:
        raise RuntimeError(f'{command!r} (exit {result.returncode})\n{result.stdout}')
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runs', type=int, default=7)
    parser.add_argument('--templates', type=Path)
    args = parser.parse_args()
    if args.runs < 1:
        parser.error('--runs must be positive')
    project = Path(__file__).resolve().parents[1]
    work = project / 'builds/optimization-study'
    work.mkdir(parents=True, exist_ok=True)
    (project / 'builds/.gdignore').touch()
    templates = (args.templates or work / 'templates').resolve()
    for name in ('linux_debug.x86_64', 'linux_release.x86_64'):
        if not (templates / name).is_file():
            parser.error(f'Missing {templates / name}; provide matching export templates')
    godot = os.environ.get('GODOT', 'godot4')
    commands = {}
    for optimization in (0, 3):
        stage = work / f'project-O{optimization}'
        shutil.copytree(project, stage, dirs_exist_ok=True,
                        ignore=shutil.ignore_patterns('builds', '.godot', 'generated', 'bin', '__pycache__'))
        config = stage / 'project.godot'
        config.write_text(config.read_text().replace('res://scenes/main.tscn', 'res://native/compare_builds.tscn'))
        env = dict(os.environ, OPT_LEVEL=str(optimization))
        print(f'Building C -O{optimization}', flush=True)
        run([str(stage / 'native/build.sh')], env=env, log=work / f'build-O{optimization}.log')
        preset = '''[preset.0]
name="Linux Benchmark"
platform="Linux"
runnable=true
export_filter="all_resources"
include_filter=""
exclude_filter="native/generated/*,native/*.c,native/*.h,native/*.py,native/*.sh"
export_path=""

[preset.0.options]
custom_template/debug="%s"
custom_template/release="%s"
binary_format/architecture="x86_64"
binary_format/embed_pck=false
''' % (templates / 'linux_debug.x86_64', templates / 'linux_release.x86_64')
        (stage / 'export_presets.cfg').write_text(preset)
        run([godot, '--headless', '--path', str(stage), '--import'], log=work / f'import-O{optimization}.log')
        commands[f'editor-O{optimization}'] = [godot, '--headless', '--path', str(stage)]
        for mode in ('debug', 'release'):
            label = f'{mode}-O{optimization}'
            output = work / label
            output.mkdir(exist_ok=True)
            binary = output / 'kinetest.x86_64'
            print(f'Exporting {label}', flush=True)
            run([godot, '--headless', '--path', str(stage), f'--export-{mode}',
                 'Linux Benchmark', str(binary)], log=work / f'export-{label}.log')
            commands[label] = [str(binary), '--headless']
    results = {label: [] for label in commands}
    rng = random.Random(42)
    for repeat in range(args.runs):
        labels = list(commands)
        rng.shuffle(labels)
        for label in labels:
            output = run(commands[label], log=work / f'run-{label}-{repeat}.log')
            row = json.loads(next(line.removeprefix('BENCHMARK_JSON ') for line in output.splitlines()
                                  if line.startswith('BENCHMARK_JSON ')))
            if row['debug_build'] != (not label.startswith('release')):
                raise RuntimeError(f'Wrong export mode: {label}: {row}')
            results[label].append(row)
        print(f'Completed round {repeat + 1}/{args.runs}', flush=True)
    fields = ['script_circles_ns', 'script_cosines_ns', 'native_circles_ns', 'native_cosines_ns']
    medians = {label: {field: statistics.median(row[field] for row in rows) for field in fields}
               for label, rows in results.items()}
    report = {'godot': run([godot, '--version']).strip(),
              'compiler': run([os.environ.get('CC', 'cc'), '--version']).splitlines()[0],
              'platform': platform.platform(), 'runs': args.runs, 'medians_ns': medians, 'raw': results}
    (work / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    lines = ['# Build optimization comparison', '',
             f"Godot {report['godot']}; {report['compiler']}.", '',
             f'Median of {args.runs} runs per configuration; each run averages 1,200 measured updates',
             'after 100 warmup updates. Headless, fixed reachable target (80,60), lengths 100/100.',
             'All numbers are nanoseconds per solve. Same C source and same GDScript source in each build.', '',
             '| Godot runtime | C optimization | Script circles | Script cosines | C circles | C cosines |',
             '|---|---|---:|---:|---:|---:|']
    for label, row in medians.items():
        mode, opt = label.split('-')
        lines.append(f'| {mode} | -{opt} | ' + ' | '.join(f'{row[field]:.1f}' for field in fields) + ' |')
    lines += ['', 'Native timings exclude Godot binding/result-allocation overhead. GDScript timings include',
              'its script loop and result indexing. C uses float32; GDScript scalar math uses float64.',
              'The C loop and solver translation units are separate, with LTO disabled at both optimization levels.',
              'Calls into the system math library still use that library\'s optimized implementation at -O0.',
              'Run order is shuffled each round. Timings are machine/load dependent; raw runs and build logs',
              'are in `godot/builds/optimization-study/`. The normal project library is unchanged.', '',
              'Reproduce with `python3 godot/native/compare_builds.py --templates /path/to/templates`.', '']
    destination = project / 'native/optimization_results.md'
    destination.write_text('\n'.join(lines))
    print('\n'.join(lines))


if __name__ == '__main__':
    main()
