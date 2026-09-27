extends SceneTree

var failures := 0


func _initialize() -> void:
	call_deferred("_run")


func _check(condition: bool, message: String) -> void:
	if not condition:
		failures += 1
		push_error(message)


func _run() -> void:
	# Typed calls exercise ptrcall; the scene's dynamic calls exercise Variant bindings.
	var native := NativeIK.new()
	var scene: Node2D = load("res://scenes/main.tscn").instantiate()
	root.add_child(scene)
	var limb: Limb = scene.get_node("Limb")
	limb.set_process(false)
	var ui: Control = scene.get_node("UI/Performance")
	var rng := RandomNumberGenerator.new()
	rng.seed = 420
	for i in range(500):
		var point := Vector2.from_angle(rng.randf_range(-PI, PI)) * rng.randf_range(31, 169)
		limb.upper_length = 100
		limb.lower_length = 70
		limb.target = point
		limb.flip_orientation = i % 2 == 0
		var result: Array = native.benchmark(point.x, point.y, 100.0, 70.0,
			-1.0 if limb.flip_orientation else 1.0, 1.0, float(i % 2))
		_check(int(result[6]) == 0 and int(result[7]) == 0, "Reachable native status")
		for method in range(2):
			if method == 0:
				limb._solve_intersecton_of_circles()
			else:
				limb._solve_law_of_cosines()
			var pose := Vector2(result[2 + method * 2], result[3 + method * 2])
			_check(pose.distance_to(limb.midjoint) < 0.01, "Native/script pose agreement")
			_check(absf(pose.length() - 100) < 0.01, "Upper link length")
			_check(absf(pose.distance_to(point) - 70) < 0.01, "Lower link length")
	for case in [[0, 0, 100, 70, 2], [200, 0, 100, 70, 3], [20, 0, 100, 70, 4],
		[80, 60, 0, 70, 1], [80, 60, 100, -1, 1], [170, 0, 100, 70, 0],
		[30, 0, 100, 70, 0], [100, 0, 100, 0, 0]]:
		var result: Array = native.benchmark(float(case[0]), float(case[1]), float(case[2]), float(case[3]), 1.0, 1.0, 0.0)
		_check(int(result[6]) == case[4] and int(result[7]) == case[4], "Boundary status %s" % [case])
		for j in range(6):
			_check(is_finite(result[j]), "Finite timing/pose at boundary")

	# Same fixed, reachable input for both languages. Run through the actual
	# scene signal and graph buffers, including a full ring-buffer wrap.
	limb.upper_length = 100
	limb.lower_length = 100
	limb.flip_orientation = false
	limb.position = limb.get_global_mouse_position() - Vector2(80, 60)
	for i in range(300):
		limb.solver = i % 2 as Limb.Solver
		limb.use_native_result = i % 4 < 2
		limb._process(0.0)
		_check(limb.last_reachability == Limb.Reachability.REACHABLE, "Scene reachable")
		_check(limb.pose_difference < 0.01, "Scene pose comparison")
		var displayed := limb.midjoint
		if limb.use_native_result:
			var result: Array = native.benchmark(80.0, 60.0, 100.0, 100.0, 1.0, 1.0, 0.0)
			_check(displayed.distance_to(Vector2(result[2 + limb.solver * 2], result[3 + limb.solver * 2])) < 0.001, "Selected native pose")
		else:
			if limb.solver == 0:
				limb._solve_intersecton_of_circles()
			else:
				limb._solve_law_of_cosines()
			_check(displayed.distance_to(limb.midjoint) < 0.001, "Selected script pose")
	_check(ui.sample_count == 240 and ui.next_sample == 60, "Circular buffer wrap")
	var cs: Vector3 = ui._stats(ui.circles)
	var ls: Vector3 = ui._stats(ui.cosines)
	var nc: Vector3 = ui._stats(ui.native_circles)
	var nl: Vector3 = ui._stats(ui.native_cosines)
	print("Godot fixed target (80,60), lengths 100/100, last 240 frames:")
	print("Circles: script %.1f ns, native %.1f ns, %.1fx faster" % [cs.y * 1000, nc.y, cs.y * 1000 / nc.y])
	print("Cosines: script %.1f ns, native %.1f ns, %.1fx faster" % [ls.y * 1000, nl.y, ls.y * 1000 / nl.y])
	print("Native cosine/circle ratio: %.2fx" % (nl.y / nc.y))
	if "--capture" in OS.get_cmdline_user_args():
		limb.use_native_result = true
		limb.position = Vector2(570, 332)
		limb.queue_redraw()
		ui.queue_redraw()
		await process_frame
		await RenderingServer.frame_post_draw
		root.get_texture().get_image().save_png("/tmp/kinetest-native.png")
	print("PASS" if failures == 0 else "FAIL: %d checks" % failures)
	quit(0 if failures == 0 else 1)
