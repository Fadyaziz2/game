extends Node

enum Screen { CONNECTING, MENU, ROOMS, WAITING, ARENA, RESULT }
const WEAPON_NAMES = ["Pistol","Fireball","Sword","Machine Gun","Fire Sword","Legendary Kick","Bazooka"]
const WEAPON_COLORS = [Color.GOLD,Color.ORANGE,Color.SKY_BLUE,Color.YELLOW,Color.RED,Color.VIOLET,Color.LIME_GREEN]

var net := ArenaNetwork.new()
var screen := Screen.CONNECTING
var world: Node3D
var player: Node3D
var rival: Node3D
var camera: Camera3D
var ui: CanvasLayer
var menu_panel: Control
var arena_ui: Control
var status_label: Label
var room_label: Label
var rooms_box: VBoxContainer
var weapon_box: HBoxContainer
var hp_player: ProgressBar
var hp_rival: ProgressBar
var round_label: Label
var joystick: VirtualJoystick
var crouching := false
var jumping := false
var jump_velocity := 0.0
var selected_weapon := 0
var send_timer := 0.0
var notice_timer := 0.0
var notice_label: Label
var current_seed := -1
var obstacles: Array[Node3D] = []

func _ready() -> void:
	net.load_config()
	_build_world()
	_build_ui()
	_connect_network_signals()
	net.connect_server()
	status_label.text = "CONNECTING TO\n%s" % net.server_url

func _process(delta: float) -> void:
	net.poll()
	if notice_timer > 0:
		notice_timer -= delta
		notice_label.visible = true
	else:
		notice_label.visible = false
	if screen == Screen.ARENA:
		_update_player(delta)
		_sync_server_state(delta)

func _connect_network_signals() -> void:
	net.connected.connect(func(): screen=Screen.MENU; _show_menu())
	net.disconnected.connect(func(): _show_error("Disconnected from server"))
	net.network_error.connect(_show_error)
	net.rooms_changed.connect(_populate_rooms)
	net.waiting_for_player.connect(func(id): screen=Screen.WAITING;_show_waiting(id))
	net.round_started.connect(_start_round)
	net.state_received.connect(_read_state)
	net.round_ended.connect(_round_end)
	net.match_completed.connect(_match_complete)

func _build_world() -> void:
	world = Node3D.new(); add_child(world)
	var env := WorldEnvironment.new(); var environment := Environment.new()
	environment.background_mode=Environment.BG_COLOR;environment.background_color=Color("09111f")
	environment.ambient_light_source=Environment.AMBIENT_SOURCE_COLOR;environment.ambient_light_color=Color("a8c7ff");environment.ambient_light_energy=0.5
	env.environment=environment;world.add_child(env)
	var sun:=DirectionalLight3D.new();sun.rotation_degrees=Vector3(-58,-30,0);sun.shadow_enabled=true;sun.light_energy=1.4;world.add_child(sun)
	var floor:=MeshInstance3D.new();var floor_mesh:=BoxMesh.new();floor_mesh.size=Vector3(96,.2,68);floor.mesh=floor_mesh;floor.position.y=-.1
	floor.material_override=_material(Color("26333e"));world.add_child(floor)
	player=_make_fighter(false);rival=_make_fighter(true);world.add_child(player);world.add_child(rival)
	player.position=Vector3(-6,0,0);rival.position=Vector3(6,0,1)
	camera=Camera3D.new();camera.fov=56;world.add_child(camera);camera.current=true
	_generate_obstacles(1)

func _make_fighter(enemy: bool) -> Node3D:
	var root:=Node3D.new();root.name = "Rival" if enemy else "Fady"
	var skin:=Color("b87955") if enemy else Color("c98e68")
	var shirt:=Color("6b192d") if enemy else Color("121721")
	_part(root,"Torso",CapsuleMesh.new(),Vector3(0,1.45,0),Vector3(.85,1.15,.62),shirt)
	_part(root,"Head",SphereMesh.new(),Vector3(0,2.45,0),Vector3(.62,.70,.62),skin)
	_part(root,"Hair",SphereMesh.new(),Vector3(0,2.64,-.03),Vector3(.57,.34,.57),Color("2b1c18"))
	_part(root,"Beard",SphereMesh.new(),Vector3(0,2.30,.19),Vector3(.43,.30,.40),Color("30201b"))
	_part(root,"LeftArm",CapsuleMesh.new(),Vector3(-.62,1.42,0),Vector3(.30,.95,.30),skin)
	_part(root,"RightArm",CapsuleMesh.new(),Vector3(.62,1.42,0),Vector3(.30,.95,.30),skin)
	_part(root,"LeftLeg",CapsuleMesh.new(),Vector3(-.27,.55,0),Vector3(.36,1.05,.36),Color("171c2b"))
	_part(root,"RightLeg",CapsuleMesh.new(),Vector3(.27,.55,0),Vector3(.36,1.05,.36),Color("171c2b"))
	return root

func _part(parent:Node3D,n:String,mesh:PrimitiveMesh,pos:Vector3,scale_v:Vector3,color:Color)->MeshInstance3D:
	var node:=MeshInstance3D.new();node.name=n;node.mesh=mesh;node.position=pos;node.scale=scale_v;node.material_override=_material(color);parent.add_child(node);return node

func _material(color:Color)->StandardMaterial3D:
	var m:=StandardMaterial3D.new();m.albedo_color=color;m.roughness=.72;return m

func _generate_obstacles(seed_value:int)->void:
	for o in obstacles:o.queue_free()
	obstacles.clear();seed(seed_value)
	for i in range(13+net.round_number*2):
		var o:=MeshInstance3D.new();var mesh:=BoxMesh.new();var low:=randf()<.45
		mesh.size=Vector3(randf_range(2.5,6.5),1.05 if low else randf_range(2.5,4.5),randf_range(1.0,2.8));o.mesh=mesh
		o.position=Vector3(randf_range(-42,42),mesh.size.y/2,randf_range(-30,30))
		if abs(o.position.x)<8 and abs(o.position.z)<7:o.position.x+=14;o.position.z+=8
		o.material_override=_material(Color("66747d") if low else Color("364451"));world.add_child(o);obstacles.append(o)

func _build_ui()->void:
	ui=CanvasLayer.new();add_child(ui)
	menu_panel=Control.new();menu_panel.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT);ui.add_child(menu_panel)
	var bg:=ColorRect.new();bg.color=Color(0.025,0.045,0.08,.94);bg.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT);menu_panel.add_child(bg)
	var title:=Label.new();title.text="FADY: ARENA ASCENSION";title.horizontal_alignment=HORIZONTAL_ALIGNMENT_CENTER;title.add_theme_font_size_override("font_size",48);title.add_theme_color_override("font_color",Color.GOLD);title.set_anchors_and_offsets_preset(Control.PRESET_TOP_WIDE);title.position.y=55;menu_panel.add_child(title)
	status_label=Label.new();status_label.horizontal_alignment=HORIZONTAL_ALIGNMENT_CENTER;status_label.add_theme_font_size_override("font_size",22);status_label.set_anchors_and_offsets_preset(Control.PRESET_CENTER_TOP);status_label.position=Vector2(-350,145);status_label.size=Vector2(700,80);menu_panel.add_child(status_label)
	room_label=Label.new();room_label.horizontal_alignment=HORIZONTAL_ALIGNMENT_CENTER;room_label.add_theme_font_size_override("font_size",42);room_label.set_anchors_and_offsets_preset(Control.PRESET_CENTER_TOP);room_label.position=Vector2(-350,230);room_label.size=Vector2(700,65);menu_panel.add_child(room_label)
	rooms_box=VBoxContainer.new();rooms_box.set_anchors_and_offsets_preset(Control.PRESET_CENTER);rooms_box.position=Vector2(-260,-60);rooms_box.size=Vector2(520,350);menu_panel.add_child(rooms_box)
	arena_ui=Control.new();arena_ui.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT);arena_ui.visible=false;ui.add_child(arena_ui)
	hp_player=_health_bar(Vector2(24,22),Color("24d78a"));hp_rival=_health_bar(Vector2(680,22),Color("ee4351"))
	round_label=Label.new();round_label.position=Vector2(545,30);round_label.size=Vector2(190,40);round_label.horizontal_alignment=HORIZONTAL_ALIGNMENT_CENTER;round_label.add_theme_font_size_override("font_size",25);arena_ui.add_child(round_label)
	weapon_box=HBoxContainer.new();weapon_box.position=Vector2(260,83);weapon_box.size=Vector2(760,58);arena_ui.add_child(weapon_box)
	joystick=VirtualJoystick.new();joystick.position=Vector2(35,485);arena_ui.add_child(joystick)
	var jump:=_action_button("JUMP",Vector2(1075,465),Color("318ce7"));jump.button_down.connect(func():if not jumping:jumping=true;jump_velocity=8.2)
	var crouch:=_action_button("CROUCH",Vector2(925,570),Color("6b5ddb"));crouch.button_down.connect(func():crouching=true);crouch.button_up.connect(func():crouching=false)
	var attack:=_action_button("ATTACK",Vector2(1080,575),Color("d63845"));attack.button_down.connect(_attack)
	notice_label=Label.new();notice_label.position=Vector2(340,180);notice_label.size=Vector2(600,62);notice_label.horizontal_alignment=HORIZONTAL_ALIGNMENT_CENTER;notice_label.add_theme_font_size_override("font_size",28);notice_label.add_theme_color_override("font_color",Color.GOLD);notice_label.visible=false;arena_ui.add_child(notice_label)
	var credit:=Label.new();credit.text="Developed by Fady Gamil";credit.position=Vector2(1000,690);credit.add_theme_font_size_override("font_size",16);arena_ui.add_child(credit)

func _health_bar(pos:Vector2,color:Color)->ProgressBar:
	var b:=ProgressBar.new();b.position=pos;b.size=Vector2(570,43);b.max_value=1000;b.value=1000;b.show_percentage=true
	var fill:=StyleBoxFlat.new();fill.bg_color=color;fill.corner_radius_top_left=12;fill.corner_radius_top_right=12;fill.corner_radius_bottom_left=12;fill.corner_radius_bottom_right=12;b.add_theme_stylebox_override("fill",fill);arena_ui.add_child(b);return b

func _action_button(text:String,pos:Vector2,color:Color)->Button:
	var b:=Button.new();b.text=text;b.position=pos;b.size=Vector2(145,96);b.add_theme_font_size_override("font_size",22)
	var s:=StyleBoxFlat.new();s.bg_color=Color(color,.8);s.corner_radius_top_left=24;s.corner_radius_top_right=24;s.corner_radius_bottom_left=24;s.corner_radius_bottom_right=24;b.add_theme_stylebox_override("normal",s);arena_ui.add_child(b);return b

func _clear_rooms()->void:
	for child in rooms_box.get_children():child.queue_free()

func _show_menu()->void:
	menu_panel.visible=true;arena_ui.visible=false;_clear_rooms();status_label.text="CONNECTED\n%s"%net.server_url;room_label.text=""
	var create:=Button.new();create.text="CREATE ROOM";create.custom_minimum_size=Vector2(520,70);create.pressed.connect(net.create_room);rooms_box.add_child(create)
	var join:=Button.new();join.text="JOIN A ROOM";join.custom_minimum_size=Vector2(520,70);join.pressed.connect(func():screen=Screen.ROOMS;net.request_rooms());rooms_box.add_child(join)

func _populate_rooms()->void:
	_clear_rooms();room_label.text="AVAILABLE ROOMS"
	if net.rooms.is_empty():
		var l:=Label.new();l.text="No waiting rooms";l.horizontal_alignment=HORIZONTAL_ALIGNMENT_CENTER;rooms_box.add_child(l)
	for room in net.rooms:
		var b:=Button.new();var id:=str(room.get("id",""));b.text="ROOM %s  •  HOST %s"%[id,str(room.get("host","Player"))];b.custom_minimum_size=Vector2(520,58);b.pressed.connect(func():net.join_room(id));rooms_box.add_child(b)
	var refresh:=Button.new();refresh.text="REFRESH";refresh.pressed.connect(net.request_rooms);rooms_box.add_child(refresh)

func _show_waiting(id:String)->void:
	_clear_rooms();room_label.text="ROOM ID: %s"%id;status_label.text="WAITING FOR SECOND PLAYER..."

func _start_round(number:int,unlocked:int)->void:
	screen=Screen.ARENA;menu_panel.visible=false;arena_ui.visible=true;round_label.text="ROUND %d / 5"%number
	if current_seed!=net.obstacle_seed:current_seed=net.obstacle_seed;_generate_obstacles(current_seed)
	_build_weapon_buttons()
	if unlocked>=0:notice_label.text="UNLOCKED: %s"%WEAPON_NAMES[unlocked];notice_timer=4

func _build_weapon_buttons()->void:
	for child in weapon_box.get_children():child.queue_free()
	for raw in net.available_weapons:
		var w:=int(raw);var b:=Button.new();b.text="%s"%WEAPON_NAMES[w];b.custom_minimum_size=Vector2(130,52);b.modulate=WEAPON_COLORS[w];b.pressed.connect(func():selected_weapon=w);weapon_box.add_child(b)
	if not net.available_weapons.has(selected_weapon):selected_weapon=int(net.available_weapons[0])

func _update_player(delta:float)->void:
	var move: Vector2 = joystick.value
var keyboard: Vector2 = Vector2.ZERO

if Input.is_key_pressed(KEY_A):
	keyboard.x -= 1.0

if Input.is_key_pressed(KEY_D):
	keyboard.x += 1.0

if Input.is_key_pressed(KEY_W):
	keyboard.y += 1.0

if Input.is_key_pressed(KEY_S):
	keyboard.y -= 1.0
	move+=keyboard.normalized() if keyboard.length()>0 else Vector2.ZERO
	move=move.limit_length(1)
	var dir:=Vector3(move.x,0,move.y)
	if dir.length()>.05:
		player.position+=dir.normalized()*(3.2 if crouching else 6.2)*delta
		player.rotation.y=atan2(dir.x,dir.z)
		_animate_walk(player,Time.get_ticks_msec()/1000.0)
	player.position.x=clamp(player.position.x,-47.0,47.0);player.position.z=clamp(player.position.z,-34.0,34.0)
	if jumping:
		jump_velocity-=19*delta;player.position.y+=jump_velocity*delta
		if player.position.y<=0:player.position.y=0;jumping=false;jump_velocity=0
	player.scale.y=.68 if crouching else 1.0
	camera.position=player.position+Vector3(-9.5,7.2,-11.5);camera.look_at(player.position+Vector3(0,1.2,0))
	send_timer-=delta
	if send_timer<=0:net.send_input(player.position,player.rotation.y,crouching);send_timer=.05

func _animate_walk(f:Node3D,t:float)->void:
	var swing:=sin(t*9)*.6
	f.get_node("LeftArm").rotation.x=swing;f.get_node("RightArm").rotation.x=-swing
	f.get_node("LeftLeg").rotation.x=-swing*.55;f.get_node("RightLeg").rotation.x=swing*.55

func _attack()->void:
	net.attack(selected_weapon)
	var arm:=player.get_node("RightArm");var tween:=create_tween();tween.tween_property(arm,"rotation:x",-1.65,.13);tween.tween_property(arm,"rotation:x",0.0,.18)

func _read_state()->void:
	var me:=net.me();var other:=net.rival()
	if not me.is_empty():hp_player.value=float(me.get("hp",1000))
	if not other.is_empty():
		hp_rival.value=float(other.get("hp",1000));rival.position=Vector3(float(other.get("x",0)),float(other.get("y",0)),float(other.get("z",0)));rival.rotation.y=float(other.get("yaw",0));rival.scale.y=.68 if bool(other.get("crouch",false)) else 1.0
	if round_label.text!="ROUND %d / 5"%net.round_number:round_label.text="ROUND %d / 5"%net.round_number;_build_weapon_buttons()

func _sync_server_state(_delta:float)->void:
	var me:=net.me()
	if not me.is_empty():
		# Health and life are always taken from the authoritative Node.js server.
		hp_player.value=float(me.get("hp",1000))

func _round_end(winner:String)->void:
	screen=Screen.RESULT;notice_label.text="ROUND WON" if winner==net.player_id else "ROUND LOST";notice_label.visible=true;notice_timer=3.4

func _match_complete(champion:String)->void:
	screen=Screen.RESULT;notice_label.text="ONLINE CHAMPION!" if champion==net.player_id else "MATCH COMPLETE";notice_label.visible=true;notice_timer=999

func _show_error(message:String)->void:
	status_label.text="NETWORK ERROR\n%s"%message;menu_panel.visible=true;arena_ui.visible=false
