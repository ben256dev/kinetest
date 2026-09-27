extends Node

# Exportable entry point: avoids --script, which release templates do not need
# to support. All variants use this identical fixed-input benchmark.
var totals := PackedFloat64Array([0.0, 0.0, 0.0, 0.0])
var samples := 0
var recording := false


func _ready() -> void:
	call_deferred("_run")


func _record(circles_us: float, cosines_us: float, circles_ns: float, cosines_ns: float) -> void:
	if recording:
		totals[0] += circles_us * 1000.0
		totals[1] += cosines_us * 1000.0
		totals[2] += circles_ns
		totals[3] += cosines_ns
		samples += 1


func _run() -> void:
	var scene: Node2D = load("res://scenes/main.tscn").instantiate()
	add_child(scene)
	var limb: Limb = scene.get_node("Limb")
	limb.set_process(false)
	if limb.native_solver == null:
		push_error("Native extension missing")
		get_tree().quit(1)
		return
	limb.upper_length = 100
	limb.lower_length = 100
	limb.flip_orientation = false
	limb.use_native_result = true
	limb.position = limb.get_global_mouse_position() - Vector2(80, 60)
	limb.timings_recorded.connect(_record)
	for frame in range(1300):
		recording = frame >= 100
		limb.solver = frame % 2 as Limb.Solver
		limb._process(0.0)
		# Explicit checks are kept in release exports (unlike assert).
		if limb.last_reachability != Limb.Reachability.REACHABLE or limb.pose_difference > 0.01:
			push_error("Benchmark input/pose mismatch")
			get_tree().quit(1)
			return
	print("BENCHMARK_JSON " + JSON.stringify({
		"debug_build": OS.is_debug_build(),
		"editor_binary": OS.has_feature("editor"),
		"frames": samples,
		"script_circles_ns": totals[0] / samples,
		"script_cosines_ns": totals[1] / samples,
		"native_circles_ns": totals[2] / samples,
		"native_cosines_ns": totals[3] / samples,
	}))
	get_tree().quit()
