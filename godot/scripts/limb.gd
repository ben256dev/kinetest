class_name Limb
extends Node2D

var midjoint: Vector2
var end_effector: Vector2
var target: Vector2

enum Solver { INTERSECTION_OF_CIRCLES, LAW_OF_COSINES }
enum Reachability { REACHABLE, INVALID_LIMB_LENGTH, ZERO_DISTANCE, TOO_FAR, TOO_CLOSE }

@export_group("Inverse Kinematics")
@export var use_native_result: bool = true
@export var solver: Solver
@export var upper_length: float = 100
@export var lower_length: float = 100
@export var flip_orientation: bool = false

@export_group("Drawing")
@export var line_color: Color = Color.WHITE
@export var line_width: float = 7.0
@export var joint_radius: float = 20.0
@export var joint_color: Color = Color.WHITE
@export var target_radius: float = 15.0
@export var target_color: Color = Color.YELLOW

signal timings_recorded(
	circles_usec: float, cosines_usec: float, native_circles_ns: float, native_cosines_ns: float
)

const BENCHMARK_CALLS := 100
const NATIVE_BENCHMARK_CALLS := 4096
var native_solver: RefCounted
var native_bridge_usec := 0
var last_reachability: Reachability
var pose_difference := 0.0
var _benchmark_frame := 0

var _u: Vector2
var _n: float


func _ready() -> void:
	if ClassDB.class_exists("NativeIK"):
		native_solver = ClassDB.instantiate("NativeIK")
	else:
		push_warning("NativeIK unavailable. Run godot/native/build.sh and restart Godot.")


func _check_reachability() -> Reachability:
	var r := upper_length
	var R := lower_length

	if r <= 0.0 || R < 0.0:
		return Reachability.INVALID_LIMB_LENGTH

	_u = target
	_n = _u.length_squared()

	if _n == 0.0:
		return Reachability.ZERO_DISTANCE

	var radii_sum: float = r + R
	if _n > radii_sum ** 2:
		return Reachability.TOO_FAR

	var radii_diff = absf(r - R)
	if _n < radii_diff ** 2:
		return Reachability.TOO_CLOSE

	return Reachability.REACHABLE


func _solve_intersecton_of_circles() -> Reachability:
	var r := upper_length
	var R := lower_length

	var reachability := _check_reachability()
	if reachability:
		return reachability

	var d: float = sqrt(_n)
	var x: float = (_n + r ** 2 - R ** 2) / (2.0 * d)
	var y: float = sqrt(maxf(0.0, r ** 2 - x ** 2)) * (-1.0 if flip_orientation else 1.0)

	var U: Vector2 = _u / d

	midjoint = Vector2(x * U.x + y * -U.y, x * U.y + y * U.x)

	return reachability


func _solve_law_of_cosines() -> Reachability:
	var r := upper_length
	var R := lower_length

	var reachability := _check_reachability()
	if reachability:
		return reachability

	var d: float = sqrt(_n)
	var numerator: float = r ** 2 + _n - R ** 2
	var denominator: float = 2.0 * r * d

	var theta: float = acos(clampf(numerator / denominator, -1.0, 1.0))
	if is_nan(theta):
		theta = 0.0

	var end_effector_angle: float = atan2(_u.y, _u.x)
	var midjoint_angle: float = end_effector_angle + theta * (-1.0 if flip_orientation else 1.0)

	midjoint = Vector2(cos(midjoint_angle), sin(midjoint_angle)) * r

	return reachability


func _process(_delta) -> void:
	var r := upper_length
	var R := lower_length

	target = to_local(get_global_mouse_position())

	# Alternate order to reduce systematic first/second-run bias. Save each
	# result so only the selected solver drives the displayed limb.
	var results: Array[Vector2] = [midjoint, midjoint]
	var statuses: Array[Reachability] = [Reachability.REACHABLE, Reachability.REACHABLE]
	var timings: Array[float] = [0.0, 0.0]
	for offset in range(2):
		var method := (_benchmark_frame + offset) % 2
		var started := Time.get_ticks_usec()
		if method == Solver.INTERSECTION_OF_CIRCLES:
			for iteration in range(BENCHMARK_CALLS):
				statuses[method] = _solve_intersecton_of_circles()
		else:
			for iteration in range(BENCHMARK_CALLS):
				statuses[method] = _solve_law_of_cosines()
		timings[method] = float(Time.get_ticks_usec() - started) / BENCHMARK_CALLS
		results[method] = midjoint
	midjoint = results[solver]
	var reachability: Reachability = statuses[solver]
	var native_timings := Vector2(-1, -1)
	if native_solver != null:
		var started := Time.get_ticks_usec()
		var native: Array = native_solver.benchmark(
			target.x,
			target.y,
			r,
			R,
			-1.0 if flip_orientation else 1.0,
			float(NATIVE_BENCHMARK_CALLS),
			float(_benchmark_frame % 2)
		)
		native_bridge_usec = Time.get_ticks_usec() - started
		native_timings = Vector2(native[0], native[1])
		var native_pose := Vector2(native[2 + solver * 2], native[3 + solver * 2])
		pose_difference = (
			native_pose.distance_to(results[solver])
			if reachability == Reachability.REACHABLE
			else 0.0
		)
		if use_native_result:
			reachability = int(native[6 + solver]) as Reachability
			if reachability == Reachability.REACHABLE:
				midjoint = native_pose
	_benchmark_frame += 1
	last_reachability = reachability
	timings_recorded.emit(timings[0], timings[1], native_timings.x, native_timings.y)

	match reachability:
		Reachability.REACHABLE:
			end_effector = target
		Reachability.TOO_FAR:
			end_effector = target.normalized() * (r + R)
			midjoint = target.normalized() * r
		Reachability.TOO_CLOSE:
			end_effector = target.normalized() * (r - R)
			midjoint = target.normalized() * r

	queue_redraw()


func _draw() -> void:
	var points = PackedVector2Array([Vector2.ZERO, midjoint, end_effector])
	draw_polyline(points, line_color, line_width)
	draw_circle(Vector2.ZERO, joint_radius, joint_color)
	draw_circle(midjoint, joint_radius, joint_color)
	draw_circle(end_effector, joint_radius, joint_color)
	draw_circle(target, target_radius, target_color)
