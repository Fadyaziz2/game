# Fady: Arena Ascension

3D online/offline arena fighting game written in C++17 and raylib. The client runs on macOS and Windows; the authoritative server runs on Node.js.

## Build on macOS

Install Xcode Command Line Tools, CMake, and Git, then run:

```bash
cd Fady_Arena_Ascension
cmake -S . -B build
cmake --build build -j
./build/FadyArenaAscension
```

## Build on Windows

Install Visual Studio 2022 with **Desktop development with C++**, CMake, and Git. In Developer PowerShell:

```powershell
cd Fady_Arena_Ascension
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
.\build\Release\FadyArenaAscension.exe
```

## Controls

| Key | Action |
|---|---|
| W A S D | Move in four directions |
| Space | Jump |
| Left Ctrl / C | Crouch |
| J | Full-body punch combo |
| K | Kick / legendary kick after round 4 |
| 1 | Pistol |
| 2 | Fireball |
| 3 | Sword (round 1) |
| 4 | Machine gun (round 2) |
| 5 | Fire sword (round 3) |
| 6 | Legendary kick (round 4) |
| 7 | Bazooka (round 5) |
| Tab | Cycle unlocked weapons |
| R | Restart current round after defeat |
| Enter | Continue after winning a round |

The arena is wider than the screen. The camera follows the player, obstacles are randomized each round, and both fighters can disappear from one another's view when separated.

Developed by Fady Gamil.

## Online server

The online mode intentionally uses plain `http://` and `ws://` for classroom traffic inspection. Do not use this configuration for passwords or production secrets.

```bash
cd server
npm install
npm start
```

The default endpoints are:

- `http://SERVER_IP:8080/health`
- `http://SERVER_IP:8080/rooms`
- `ws://SERVER_IP:8080/game`

Edit `server/server_config.json` to change the server port. On every player's computer, edit `client_config.txt`:

```ini
server_url=ws://YOUR_SERVER_IP:8080/game
player_name=Fady
```

Start the game and choose:

1. `O` — Play Online.
2. `C` — Create Room, or `J` — Join Room.
3. The host shares the six-character Room ID.
4. The second player selects the visible room by pressing its number.
5. The server starts five rounds after both players connect.

The Node.js server is authoritative for health, damage, alive/dead state, weapon cooldowns, round state, wins, and randomized weapon unlock order. Both players always begin with pistol and fireball. One of the remaining five weapons is unlocked each round in a server-randomized order; by round five all weapons are available.

## Classroom packet inspection

Because the lab uses cleartext HTTP/WebSocket traffic, students can filter Wireshark with:

```text
tcp.port == 8080
```

The WebSocket JSON messages show room creation, joining, movement inputs, authoritative state snapshots, attack requests, exact server-calculated damage, health, and match results. Change to `https://` and `wss://` before any real public deployment.
