extends Control

const CAPACITY := 240
const CIRCLES_COLOR := Color(0.25, 0.85, 1.0)
const COSINES_COLOR := Color(1.0, 0.65, 0.25)
var circles := PackedFloat64Array()
var cosines := PackedFloat64Array()
var native_circles := PackedFloat64Array()
var native_cosines := PackedFloat64Array()
var next_sample := 0
var sample_count := 0
@onready var limb: Limb = get_node("../../Limb")


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	circles.resize(CAPACITY)
	cosines.resize(CAPACITY)
	native_circles.resize(CAPACITY)
	native_cosines.resize(CAPACITY)
	limb.timings_recorded.connect(_record)


func _record(circles_usec: float, cosines_usec: float, native_circles_ns: float, native_cosines_ns: float) -> void:
	circles[next_sample] = circles_usec
	cosines[next_sample] = cosines_usec
	native_circles[next_sample] = native_circles_ns
	native_cosines[next_sample] = native_cosines_ns
	next_sample = (next_sample + 1) % CAPACITY
	sample_count = mini(sample_count + 1, CAPACITY)
	queue_redraw()


func _label(at: Vector2, value: String, color := Color.WHITE, font_size := 16) -> void:
	draw_string(ThemeDB.fallback_font, at, value, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, color)


# Current, rolling mean, rolling maximum. No allocation in the timed solvers.
func _stats(values: PackedFloat64Array) -> Vector3:
	var total := 0.0
	var peak := 0.0
	for i in range(sample_count):
		total += values[i]
		peak = maxf(peak, values[i])
	return Vector3(values[(next_sample - 1 + CAPACITY) % CAPACITY], total / sample_count, peak)


func _draw() -> void:
	if sample_count == 0:
		return
	var cs := _stats(circles)
	var ls := _stats(cosines)
	var nc := _stats(native_circles)
	var nl := _stats(native_cosines)
	var native_available := limb.native_solver != null
	var backend := "Native C" if native_available and limb.use_native_result else "GDScript"
	var selected := "Circles" if limb.solver == Limb.Solver.INTERSECTION_OF_CIRCLES else "Law of cosines"
	_label(Vector2(24, 30), "Pose: %s / %s | Both solvers benchmarked" % [backend, selected])
	_label(Vector2(24, 54), "GDScript circles: %.2f us | mean %.2f | max %.2f" % [cs.x, cs.y, cs.z], CIRCLES_COLOR)
	_label(Vector2(24, 78), "GDScript cosines: %.2f us | mean %.2f | max %.2f" % [ls.x, ls.y, ls.z], COSINES_COLOR)
	if native_available:
		_label(Vector2(24, 106), "Native C circles: %.1f ns | mean %.1f | max %.1f" % [nc.x, nc.y, nc.z], CIRCLES_COLOR)
		_label(Vector2(24, 130), "Native C cosines: %.1f ns | mean %.1f | max %.1f" % [nl.x, nl.y, nl.z], COSINES_COLOR)
		_label(Vector2(24, 156), "Mean speedup: circles %.1fx / cosines %.1fx | C cosines/circles: %.2fx" % [cs.y * 1000 / maxf(nc.y, 0.001), ls.y * 1000 / maxf(nl.y, 0.001), nl.y / maxf(nc.y, 0.001)], Color.WHITE, 14)
	else:
		_label(Vector2(24, 110), "Native C unavailable: run godot/native/build.sh, then restart Godot.", COSINES_COLOR, 14)
	_label(Vector2(24, 180), "%d script / %d C calls per solver per frame; C timer excludes engine bridge." % [Limb.BENCHMARK_CALLS, Limb.NATIVE_BENCHMARK_CALLS], Color(0.7, 0.75, 0.8), 14)
	var reach := "Reachable" if limb.last_reachability == Limb.Reachability.REACHABLE else "Early return: %s" % Limb.Reachability.keys()[limb.last_reachability]
	_label(Vector2(24, 202), "%s | Full C batch + bridge: %d us | Pose difference: %.5f px" % [reach, limb.native_bridge_usec, limb.pose_difference], Color(0.7, 0.75, 0.8), 14)

	# Separate scales keep the much smaller native timings readable.
	var panel_width := maxf(220, (size.x - 36) / 2)
	var top := maxf(224, size.y - 220)
	_graph(Rect2(12, top, panel_width, 208), "GDScript (us/call)", circles, cosines, maxf(cs.z, ls.z), 1.0)
	if native_available:
		_graph(Rect2(24 + panel_width, top, panel_width, 208), "Native C (ns/call)", native_circles, native_cosines, maxf(nc.z, nl.z), 10.0)


func _graph(panel: Rect2, title: String, circle_values: PackedFloat64Array, cosine_values: PackedFloat64Array, peak: float, minimum: float) -> void:
	draw_rect(panel, Color(0.04, 0.05, 0.07, 0.94))
	_label(panel.position + Vector2(14, 24), "%s | %d/%d frames" % [title, sample_count, CAPACITY], Color.WHITE, 14)
	var plot := Rect2(panel.position + Vector2(64, 42), panel.size - Vector2(84, 76))
	var ceiling := maxf(minimum, peak * 1.1)
	for tick in range(3):
		var fraction := tick / 2.0
		var y := plot.end.y - plot.size.y * fraction
		draw_line(Vector2(plot.position.x, y), Vector2(plot.end.x, y), Color(0.25, 0.29, 0.35))
		_label(Vector2(panel.position.x + 8, y + 5), "%.1f" % (ceiling * fraction), Color(0.7, 0.75, 0.8), 12)
	var oldest := next_sample if sample_count == CAPACITY else 0
	var circle_points := PackedVector2Array()
	var cosine_points := PackedVector2Array()
	for i in range(sample_count):
		var index := (oldest + i) % CAPACITY
		var x := plot.end.x - float(sample_count - 1 - i) / (CAPACITY - 1) * plot.size.x
		circle_points.append(Vector2(x, plot.end.y - circle_values[index] / ceiling * plot.size.y))
		cosine_points.append(Vector2(x, plot.end.y - cosine_values[index] / ceiling * plot.size.y))
	if sample_count > 1:
		draw_polyline(circle_points, CIRCLES_COLOR, 1.5, true)
		draw_polyline(cosine_points, COSINES_COLOR, 1.5, true)
	_label(Vector2(plot.position.x, panel.end.y - 12), "239 frames ago", Color(0.7, 0.75, 0.8), 12)
	_label(Vector2(plot.end.x - 24, panel.end.y - 12), "Now", Color(0.7, 0.75, 0.8), 12)
