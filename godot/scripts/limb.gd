class_name Limb
extends Node2D

var mid_joint: Vector2
var end_effector: Vector2
var target: Vector2

@export_group("Limb Parameters")
@export var r: float = 100
@export var R: float = 100

@export_group("Drawing")
@export var line_color: Color = Color.WHITE
@export var line_width: float = 7.0
@export var joint_radius: float = 20.0
@export var joint_color: Color = Color.WHITE
@export var target_radius: float = 15.0
@export var target_color: Color = Color.YELLOW

enum Reachability { REACHABLE, INVALID_LIMB_LENGTH, ZERO_DISTANCE, TOO_FAR, TOO_CLOSE }

var _u: Vector2
var _n: float


func _check_reachability() -> Reachability:
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


func _solve_with_circles() -> Reachability:
	var reachability := _check_reachability()
	if reachability:
		return reachability

	var d: float = sqrt(_n)
	var x: float = (_n + r ** 2 - R ** 2) / (2.0 * d)
	var y: float = sqrt(r ** 2 - x ** 2)

	var U: Vector2 = _u / d

	mid_joint = Vector2(x * U.x + y * -U.y, x * U.y + y * U.x)

	return reachability


func _process(_delta) -> void:
	target = to_local(get_global_mouse_position())
	var reachability: Reachability = _solve_with_circles()
	match reachability:
		Reachability.REACHABLE:
			end_effector = target
		Reachability.TOO_FAR:
			end_effector = target.normalized() * (r + R)
			mid_joint = target.normalized() * r
		Reachability.TOO_CLOSE:
			end_effector = target.normalized() * (r - R)
			mid_joint = target.normalized() * r

	queue_redraw()


func _draw() -> void:
	var points = PackedVector2Array([Vector2.ZERO, mid_joint, end_effector])
	draw_polyline(points, line_color, line_width)
	draw_circle(Vector2.ZERO, joint_radius, joint_color)
	draw_circle(mid_joint, joint_radius, joint_color)
	draw_circle(end_effector, joint_radius, joint_color)
	draw_circle(target, target_radius, target_color)
