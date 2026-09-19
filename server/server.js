const http = require('http');
const crypto = require('crypto');
const { WebSocketServer, WebSocket } = require('ws');
const config = require('./server_config.json');

// The server is authoritative for health, damage, alive/dead state, rounds and unlock order.
const WEAPONS = {
  0: { name: 'PISTOL', damage: 32, cooldown: 340, range: 28 },
  1: { name: 'FIREBALL', damage: 48, cooldown: 700, range: 24 },
  2: { name: 'SWORD', damage: 72, cooldown: 620, range: 3.4 },
  3: { name: 'MACHINE GUN', damage: 18, cooldown: 100, range: 32 },
  4: { name: 'FIRE SWORD', damage: 105, cooldown: 720, range: 4.0 },
  5: { name: 'LEGENDARY KICK', damage: 150, cooldown: 1050, range: 4.5 },
  6: { name: 'BAZOOKA', damage: 220, cooldown: 1650, range: 38 }
};

const rooms = new Map();
const id = (n = 6) => crypto.randomBytes(n).toString('hex').slice(0, n).toUpperCase();
const shuffle = a => {
  a = [...a];
  for (let i = a.length - 1; i > 0; i--) {
    const j = crypto.randomInt(i + 1); [a[i], a[j]] = [a[j], a[i]];
  }
  return a;
};
const json = (ws, data) => ws.readyState === WebSocket.OPEN && ws.send(JSON.stringify(data));
const roomList = () => [...rooms.values()].filter(r => r.phase === 'waiting').map(r => ({
  id: r.id, host: r.players[0]?.name || 'Unknown', players: r.players.length
}));
const publicPlayer = p => ({ id:p.id, name:p.name, x:p.x, y:p.y, z:p.z, yaw:p.yaw,
  crouch:p.crouch, hp:p.hp, alive:p.alive, weapon:p.weapon, wins:p.wins });
const availableFor = room => [0, 1, ...room.weaponOrder.slice(0, room.round)];

function broadcast(room, data) { room.players.forEach(p => json(p.ws, data)); }
function snapshot(room) {
  return { type:'state', roomId:room.id, phase:room.phase, round:room.round,
    weaponOrder:room.weaponOrder, availableWeapons:availableFor(room), obstacleSeed:room.obstacleSeed,
    players:room.players.map(publicPlayer), roundWinner:room.roundWinner || null };
}
function resetRound(room) {
  room.obstacleSeed = crypto.randomInt(1, 2147483646);
  room.players.forEach((p, i) => Object.assign(p, {
    x:i ? 6 : -6, y:0, z:i ? 1 : 0, hp:1000, alive:true, lastAttack:0, weapon:0
  }));
  room.roundWinner = null; room.phase = 'playing';
  broadcast(room, { type:'round_start', round:room.round, availableWeapons:availableFor(room), obstacleSeed:room.obstacleSeed,
    unlockedWeapon:room.weaponOrder[room.round - 1] });
}
function finishRound(room, winner) {
  if (room.phase !== 'playing') return;
  winner.wins++; room.roundWinner = winner.id; room.phase = 'round_end';
  broadcast(room, { type:'round_end', round:room.round, winnerId:winner.id, wins:room.players.map(p=>({id:p.id,wins:p.wins})) });
  setTimeout(() => {
    if (!rooms.has(room.id) || room.players.length !== 2) return;
    if (room.round >= 5) {
      room.phase = 'complete';
      const champion = [...room.players].sort((a,b)=>b.wins-a.wins)[0];
      broadcast(room, { type:'match_complete', championId:champion.id, players:room.players.map(publicPlayer) });
    } else { room.round++; resetRound(room); }
  }, 3500);
}
function makePlayer(ws, name) {
  return { id:id(8), ws, name:String(name||'Player').slice(0,18), x:0,y:0,z:0,yaw:0,crouch:false,
    hp:1000, alive:true, weapon:0, wins:0, lastInput:Date.now(), lastAttack:0 };
}
function leave(ws) {
  const room = rooms.get(ws.roomId); if (!room) return;
  room.players = room.players.filter(p => p.ws !== ws);
  if (!room.players.length) rooms.delete(room.id);
  else { room.phase='waiting'; broadcast(room,{type:'peer_left'}); }
}

const server = http.createServer((req,res) => {
  res.setHeader('Access-Control-Allow-Origin','*');
  res.setHeader('Content-Type','application/json');
  if (req.url === '/health') return res.end(JSON.stringify({ok:true, rooms:rooms.size}));
  if (req.url === '/rooms') return res.end(JSON.stringify({rooms:roomList()}));
  res.statusCode=404; res.end(JSON.stringify({error:'Not found'}));
});
const wss = new WebSocketServer({ server, path:'/game' });

wss.on('connection', ws => {
  json(ws,{type:'hello',protocol:1});
  ws.on('message', raw => {
    let m; try { m=JSON.parse(raw.toString()); } catch { return json(ws,{type:'error',message:'Invalid JSON'}); }
    if (m.type==='list_rooms') return json(ws,{type:'rooms',rooms:roomList()});
    if (m.type==='create_room') {
      leave(ws); if(rooms.size>=config.maxRooms) return json(ws,{type:'error',message:'Room limit reached'});
      const room={id:id(),phase:'waiting',round:1,roundWinner:null,obstacleSeed:1,weaponOrder:shuffle([2,3,4,5,6]),players:[]};
      const p=makePlayer(ws,m.name); room.players.push(p); rooms.set(room.id,room); ws.roomId=room.id; ws.playerId=p.id;
      return json(ws,{type:'room_created',roomId:room.id,playerId:p.id,weaponOrder:room.weaponOrder});
    }
    if (m.type==='join_room') {
      leave(ws); const room=rooms.get(String(m.roomId||'').toUpperCase());
      if(!room||room.phase!=='waiting'||room.players.length>=2) return json(ws,{type:'error',message:'Room unavailable'});
      const p=makePlayer(ws,m.name); room.players.push(p); ws.roomId=room.id; ws.playerId=p.id;
      room.players.forEach((q,i)=>{q.x=i?6:-6;q.z=i?1:0;});
      json(ws,{type:'room_joined',roomId:room.id,playerId:p.id,weaponOrder:room.weaponOrder});
      broadcast(room,{type:'match_ready',roomId:room.id,players:room.players.map(publicPlayer),weaponOrder:room.weaponOrder});
      return setTimeout(()=>resetRound(room),900);
    }
    const room=rooms.get(ws.roomId); const p=room?.players.find(q=>q.ws===ws); if(!room||!p) return;
    if (m.type==='input' && room.phase==='playing' && p.alive) {
      const now=Date.now(), dt=Math.min((now-p.lastInput)/1000,.25), max=7.0*dt+.25;
      const nx=Number(m.x), nz=Number(m.z);
      if(Number.isFinite(nx)&&Number.isFinite(nz)) {
        const dx=nx-p.x,dz=nz-p.z,len=Math.hypot(dx,dz),scale=len>max?max/len:1;
        p.x=Math.max(-47,Math.min(47,p.x+dx*scale)); p.z=Math.max(-34,Math.min(34,p.z+dz*scale));
      }
      p.y=Math.max(0,Math.min(4,Number(m.y)||0)); p.yaw=Number(m.yaw)||0; p.crouch=!!m.crouch; p.lastInput=now;
      return;
    }
    if (m.type==='attack' && room.phase==='playing' && p.alive) {
      const weapon=Number(m.weapon), info=WEAPONS[weapon], allowed=availableFor(room);
      if(!info||!allowed.includes(weapon)) return json(ws,{type:'attack_rejected',reason:'weapon_locked'});
      const now=Date.now(); if(now-p.lastAttack<info.cooldown) return;
      p.lastAttack=now;p.weapon=weapon;
      const target=room.players.find(q=>q!==p&&q.alive); if(!target) return;
      const distance=Math.hypot(target.x-p.x,target.z-p.z);
      if(distance<=info.range) {
        target.hp=Math.max(0,target.hp-info.damage); target.alive=target.hp>0;
        broadcast(room,{type:'damage',attackerId:p.id,targetId:target.id,weapon,damage:info.damage,hp:target.hp,alive:target.alive});
        if(!target.alive) finishRound(room,p);
      }
    }
  });
  ws.on('close',()=>leave(ws)); ws.on('error',()=>leave(ws));
});

setInterval(()=>rooms.forEach(r=>{if(r.phase==='playing')broadcast(r,snapshot(r));}),1000/config.tickRate);
server.listen(config.port,config.host,()=>console.log(`Fady Arena server: http://${config.host}:${config.port}  ws://HOST:${config.port}/game`));
