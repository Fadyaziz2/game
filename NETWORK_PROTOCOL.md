# Fady Arena network protocol

Transport: WebSocket over cleartext HTTP (`ws://`). Messages are UTF-8 JSON.

## Client to server

- `{"type":"list_rooms"}`
- `{"type":"create_room","name":"Fady"}`
- `{"type":"join_room","roomId":"A1B2C3","name":"Player 2"}`
- `{"type":"input","x":1.2,"y":0,"z":4.5,"yaw":1.57,"crouch":false}`
- `{"type":"attack","weapon":6}`

## Server to client

- `rooms`: visible waiting rooms.
- `room_created`, `room_joined`, `match_ready`: lobby lifecycle.
- `round_start`: round number, shared obstacle seed, available weapons and the newly unlocked weapon.
- `state`: authoritative player health/alive state, positions, chosen weapons, wins and round state.
- `damage`: attacker, target, weapon, exact damage, remaining health and alive state.
- `round_end`, `match_complete`: results calculated by the server.

## Authoritative checks

The server rejects locked weapons, applies per-weapon cooldowns and ranges, clamps reported movement speed, owns health, and decides death and round completion. The client renders local prediction but the next server snapshot is authoritative.

This protocol intentionally has no encryption or authentication because it is a closed classroom demonstration. It must be upgraded to TLS (`wss://`) plus authenticated sessions before public production use.
