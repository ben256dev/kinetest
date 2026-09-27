# Godot workspace

Open `project.godot` with Godot 4.7, or run from the repository root:

```bash
godot4 --editor --path godot
```

Press F6 to preview the open scene or F5 to run the main scene.

- `scenes/main.tscn`: main 2D workspace with a limb instance and status label.
- `scenes/templates/two_link_limb.tscn`: reusable scene with Base, Midjoint, and EndEffector markers. All markers start at the origin; set their positions when implementing the solver.
- `scripts/`: space for future solver and benchmark scripts.
- `resources/`: space for future input presets and other Godot resources.
- `assets/`: space for visual assets.

This scaffold contains no scripts or solver implementation. Marker2D nodes are editor guides and do not draw a limb at runtime. The original C benchmark remains in the repository root for reference. No native extension or language binding has been chosen.
