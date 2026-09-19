#pragma once
#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <atomic>
#include <fstream>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

struct RoomView { std::string id, host; int players=0; };
struct NetPlayer { std::string id,name; float x=0,y=0,z=0,yaw=0,hp=1000; bool crouch=false,alive=true; int weapon=0,wins=0; };

class NetworkClient {
    ix::WebSocket socket;
    std::mutex mutex;
    std::queue<nlohmann::json> inbox;
public:
    std::atomic<bool> connected{false};
    bool waiting=false, playing=false, roundEnded=false, complete=false;
    std::string url="ws://127.0.0.1:8080/game", playerName="Fady", roomId, playerId, error;
    int round=1, unlockedWeapon=-1, obstacleSeed=1;
    std::string roundWinner, championId;
    std::vector<int> weaponOrder, available{0,1};
    std::vector<RoomView> rooms;
    std::vector<NetPlayer> players;

    void loadConfig(const std::string& path) {
        std::ifstream f(path); std::string line;
        while(std::getline(f,line)) {
            if(line.rfind("server_url=",0)==0) url=line.substr(11);
            else if(line.rfind("player_name=",0)==0) playerName=line.substr(12);
        }
    }
    void connect() {
        error.clear(); socket.setUrl(url);
        socket.setOnMessageCallback([this](const ix::WebSocketMessagePtr& m){
            if(m->type==ix::WebSocketMessageType::Open) connected=true;
            else if(m->type==ix::WebSocketMessageType::Close){connected=false;playing=false;}
            else if(m->type==ix::WebSocketMessageType::Error){std::lock_guard<std::mutex> lock(mutex);inbox.push({{"type","error"},{"message",m->errorInfo.reason}});connected=false;}
            else if(m->type==ix::WebSocketMessageType::Message){
                try { auto j=nlohmann::json::parse(m->str); std::lock_guard<std::mutex> lock(mutex); inbox.push(std::move(j)); } catch(...) {}
            }
        }); socket.start();
    }
    void stop(){socket.stop();connected=false;}
    void send(const nlohmann::json& j){if(connected)socket.send(j.dump());}
    void listRooms(){send({{"type","list_rooms"}});}
    void createRoom(){send({{"type","create_room"},{"name",playerName}});}
    void joinRoom(const std::string& id){send({{"type","join_room"},{"roomId",id},{"name",playerName}});}
    void input(float x,float y,float z,float yaw,bool crouch){send({{"type","input"},{"x",x},{"y",y},{"z",z},{"yaw",yaw},{"crouch",crouch}});}
    void attack(int weapon){send({{"type","attack"},{"weapon",weapon}});}

    void pump() {
        std::queue<nlohmann::json> local; {std::lock_guard<std::mutex> lock(mutex);std::swap(local,inbox);}
        while(!local.empty()) {
            auto j=std::move(local.front());local.pop();std::string t=j.value("type","");
            if(t=="rooms") {rooms.clear();for(auto&r:j["rooms"])rooms.push_back({r.value("id",""),r.value("host",""),r.value("players",0)});}
            else if(t=="room_created"||t=="room_joined") {roomId=j.value("roomId","");playerId=j.value("playerId","");waiting=true;weaponOrder=j.value("weaponOrder",std::vector<int>{});}
            else if(t=="match_ready") waiting=true;
            else if(t=="round_start") {round=j.value("round",1);available=j.value("availableWeapons",std::vector<int>{0,1});unlockedWeapon=j.value("unlockedWeapon",-1);obstacleSeed=j.value("obstacleSeed",1);playing=true;waiting=false;roundEnded=false;}
            else if(t=="state") {
                round=j.value("round",round);available=j.value("availableWeapons",available);obstacleSeed=j.value("obstacleSeed",obstacleSeed);players.clear();
                for(auto&p:j["players"])players.push_back({p.value("id",""),p.value("name",""),p.value("x",0.f),p.value("y",0.f),p.value("z",0.f),p.value("yaw",0.f),p.value("hp",1000.f),p.value("crouch",false),p.value("alive",true),p.value("weapon",0),p.value("wins",0)});
            }
            else if(t=="damage") {for(auto&p:players)if(p.id==j.value("targetId","")){p.hp=j.value("hp",p.hp);p.alive=j.value("alive",true);}}
            else if(t=="round_end") {playing=false;roundEnded=true;roundWinner=j.value("winnerId","");}
            else if(t=="match_complete") {playing=false;roundEnded=false;complete=true;championId=j.value("championId","");}
            else if(t=="peer_left") {playing=false;waiting=true;error="Other player disconnected";}
            else if(t=="error") error=j.value("message","Server error");
        }
    }
    NetPlayer* me(){for(auto& p:players)if(p.id==playerId)return &p;return nullptr;}
    NetPlayer* rival(){for(auto& p:players)if(p.id!=playerId)return &p;return nullptr;}
};
