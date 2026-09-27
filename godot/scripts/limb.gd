class_name Limb
extends Node2D

var midjoint: Vector2
var end_effector: Vector2
var target: Vector2

enum Solver { INTERSECTION_OF_CIRCLES, LAW_OF_COSINES }
enum Reachability { REACHABLE, INVALID_LIMB_LENGTH, ZERO_DISTANCE, TOO_FAR, TOO_CLOSE }

@export_group("Inverse Kinematics")
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

var _u: Vector2
var _n: float


func _check_reachability() -> Reachability:
	var r := upper_length
	var R := lower_length

	if r <= 0.0 || R < 0.0:
		return Reachability.INVALID_LIMB_LENGTH

	_u = target
	_n = _u.x ** 2 + _u.y ** 2

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
	var y: float = sqrt(r ** 2 - x ** 2) * (-1.0 if flip_orientation else 1.0)

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

	var theta: float = acos(numerator / denominator)
	if is_nan(theta):
		theta = 0.0

	var end_effector_angle: float = atan2(_u.y, _u.x)
	var midjoint_angle: float = end_effector_angle + theta

	midjoint = Vector2(cos(midjoint_angle), sin(midjoint_angle)) * r

	return reachability


func _process(_delta) -> void:
	var r := upper_length
	var R := lower_length

	target = to_local(get_global_mouse_position())

	var reachability: Reachability
	match solver:
		Solver.INTERSECTION_OF_CIRCLES:
			reachability = _solve_intersecton_of_circles()
		Solver.LAW_OF_COSINES:
			reachability = _solve_law_of_cosines()

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
