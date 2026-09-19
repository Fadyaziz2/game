class_name ArenaNetwork
extends RefCounted

signal connected
signal disconnected
signal rooms_changed
signal room_created(room_id: String)
signal waiting_for_player(room_id: String)
signal round_started(round_number: int, unlocked_weapon: int)
signal state_received
signal round_ended(winner_id: String)
signal match_completed(champion_id: String)
signal network_error(message: String)

var socket := WebSocketPeer.new()
var server_url := "ws://158.220.122.81:8089/game"
var player_name := "Fady Mobile"
var is_connected := false
var room_id := ""
var player_id := ""
var rooms: Array = []
var players: Array = []
var available_weapons: Array = [0, 1]
var weapon_order: Array = []
var round_number := 1
var obstacle_seed := 1

func load_config() -> void:
	if not FileAccess.file_exists("res://server_config.json"):
		return
	var data = JSON.parse_string(FileAccess.get_file_as_string("res://server_config.json"))
	if data is Dictionary:
		server_url = str(data.get("server_url", server_url))
		player_name = str(data.get("player_name", player_name))

func connect_server() -> void:
	if socket.get_ready_state() != WebSocketPeer.STATE_CLOSED:
		socket.close()
	socket = WebSocketPeer.new()
	var err := socket.connect_to_url(server_url)
	if err != OK:
		network_error.emit("Connection start failed: %s" % error_string(err))

func poll() -> void:
	socket.poll()
	var state := socket.get_ready_state()
	if state == WebSocketPeer.STATE_OPEN and not is_connected:
		is_connected = true
		connected.emit()
	elif state == WebSocketPeer.STATE_CLOSED and is_connected:
		is_connected = false
		disconnected.emit()
	while socket.get_available_packet_count() > 0:
		var text := socket.get_packet().get_string_from_utf8()
		var message = JSON.parse_string(text)
		if message is Dictionary:
			_handle(message)

func send_message(data: Dictionary) -> void:
	if socket.get_ready_state() == WebSocketPeer.STATE_OPEN:
		socket.send_text(JSON.stringify(data))

func request_rooms() -> void:
	send_message({"type":"list_rooms"})

func create_room() -> void:
	send_message({"type":"create_room", "name":player_name})

func join_room(id: String) -> void:
	send_message({"type":"join_room", "roomId":id.to_upper(), "name":player_name})

func send_input(pos: Vector3, yaw: float, crouch: bool) -> void:
	send_message({"type":"input", "x":pos.x, "y":pos.y, "z":pos.z, "yaw":yaw, "crouch":crouch})

func attack(weapon: int) -> void:
	send_message({"type":"attack", "weapon":weapon})

func me() -> Dictionary:
	for p in players:
		if str(p.get("id", "")) == player_id:
			return p
	return {}

func rival() -> Dictionary:
	for p in players:
		if str(p.get("id", "")) != player_id:
			return p
	return {}

func _handle(m: Dictionary) -> void:
	var type := str(m.get("type", ""))
	match type:
		"rooms":
			rooms = m.get("rooms", [])
			rooms_changed.emit()
		"room_created", "room_joined":
			room_id = str(m.get("roomId", ""))
			player_id = str(m.get("playerId", ""))
			weapon_order = m.get("weaponOrder", [])
			room_created.emit(room_id)
			waiting_for_player.emit(room_id)
		"match_ready":
			waiting_for_player.emit(str(m.get("roomId", room_id)))
		"round_start":
			round_number = int(m.get("round", 1))
			available_weapons = m.get("availableWeapons", [0,1])
			obstacle_seed = int(m.get("obstacleSeed", 1))
			round_started.emit(round_number, int(m.get("unlockedWeapon", -1)))
		"state":
			round_number = int(m.get("round", round_number))
			available_weapons = m.get("availableWeapons", available_weapons)
			obstacle_seed = int(m.get("obstacleSeed", obstacle_seed))
			players = m.get("players", [])
			state_received.emit()
		"round_end":
			round_ended.emit(str(m.get("winnerId", "")))
		"match_complete":
			match_completed.emit(str(m.get("championId", "")))
		"peer_left":
			network_error.emit("The other player disconnected")
		"error":
			network_error.emit(str(m.get("message", "Server error")))

