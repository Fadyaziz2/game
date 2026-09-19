class_name VirtualJoystick
extends Control

var value := Vector2.ZERO
var touch_id := -1
var center := Vector2.ZERO
var radius := 78.0

func _ready() -> void:
	custom_minimum_size = Vector2(190,190)
	mouse_filter = Control.MOUSE_FILTER_STOP

func _gui_input(event: InputEvent) -> void:
	if event is InputEventScreenTouch:
		if event.pressed and touch_id == -1:
			touch_id = event.index
			center = event.position
			_update_value(event.position)
		elif not event.pressed and event.index == touch_id:
			touch_id = -1; value = Vector2.ZERO; queue_redraw()
	elif event is InputEventScreenDrag and event.index == touch_id:
		_update_value(event.position)
	elif event is InputEventMouseButton:
		if event.pressed:
			touch_id = -2; center = event.position; _update_value(event.position)
		else:
			touch_id = -1; value = Vector2.ZERO; queue_redraw()
	elif event is InputEventMouseMotion and touch_id == -2:
		_update_value(event.position)

func _update_value(p: Vector2) -> void:
	value = ((p-center)/radius).limit_length(1.0)
	queue_redraw()

func _draw() -> void:
	var c := size/2.0
	draw_circle(c, radius, Color(0.08,0.12,0.18,0.72))
	draw_arc(c,radius,0,TAU,48,Color(0.3,0.72,1.0,0.85),4)
	draw_circle(c+value*radius,32,Color(0.25,0.72,1.0,0.92))

