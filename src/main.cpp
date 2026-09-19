#include <ixwebsocket/IXNetSystem.h>
#include "raylib.h"
#include "raymath.h"
#include "network_client.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

constexpr int SCREEN_W = 1440;
constexpr int SCREEN_H = 900;
constexpr float WORLD_HALF = 48.0f;
constexpr float FLOOR_Y = 0.0f;
constexpr float MAX_HP = 1000.0f;

enum class GameState { Playing, RoundWon, Defeated, Complete };
enum class Screen { MainMenu, OnlineMenu, RoomBrowser, WaitingRoom, Arena };
enum class Anim { Idle, Walk, Jump, Crouch, Punch, Kick, Hit };
enum class Weapon { Pistol, Fireball, Sword, MachineGun, FireSword, LegendaryKick, Bazooka };

struct WeaponInfo {
    const char* name;
    int key;
    float damage;
    float cooldown;
    float range;
    Color color;
};

static const std::array<WeaponInfo, 7> WEAPONS {{
    {"PISTOL", KEY_ONE, 32, 0.34f, 28, GOLD},
    {"FIREBALL", KEY_TWO, 48, 0.70f, 24, ORANGE},
    {"SWORD", KEY_THREE, 72, 0.62f, 3.4f, SKYBLUE},
    {"MACHINE GUN", KEY_FOUR, 18, 0.10f, 32, YELLOW},
    {"FIRE SWORD", KEY_FIVE, 105, 0.72f, 4.0f, RED},
    {"LEGENDARY KICK", KEY_SIX, 150, 1.05f, 4.5f, VIOLET},
    {"BAZOOKA", KEY_SEVEN, 220, 1.65f, 38, LIME}
}};

struct Fighter {
    Vector3 pos{};
    Vector3 vel{};
    float yaw = 0;
    float hp = MAX_HP;
    float cooldown = 0;
    float animTime = 0;
    float invuln = 0;
    int weapon = 0;
    Anim anim = Anim::Idle;
    bool grounded = true;
    bool crouching = false;
    bool enemy = false;
    Color shirt = BLACK;
    Color accent = WHITE;
};

struct Obstacle {
    BoundingBox box{};
    bool low = false; // low walls protect a crouching fighter
    Color color{};
};

struct Projectile {
    Vector3 pos{};
    Vector3 vel{};
    float radius = .12f;
    float damage = 10;
    float life = 2;
    int kind = 0;
    bool fromEnemy = false;
    Color color{};
};

static float Clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
static float EaseOut(float t) { t = Clamp01(t); return 1.0f - (1.0f-t)*(1.0f-t); }
static Vector3 V3(float x, float y, float z) { return {x,y,z}; }
static Vector3 Forward(float yaw) { return {sinf(yaw), 0, cosf(yaw)}; }

static bool SphereHitsBox(Vector3 p, float r, const BoundingBox& b) {
    Vector3 q{ std::clamp(p.x,b.min.x,b.max.x), std::clamp(p.y,b.min.y,b.max.y), std::clamp(p.z,b.min.z,b.max.z) };
    return Vector3DistanceSqr(p,q) <= r*r;
}

static bool SegmentBlocked(Vector3 a, Vector3 b, const std::vector<Obstacle>& obs, float shotHeight) {
    Vector3 d = Vector3Subtract(b,a);
    float len = Vector3Length(d);
    if (len < .01f) return false;
    Ray ray{a, Vector3Scale(d,1.0f/len)};
    for (const auto& o: obs) {
        if (o.low && shotHeight > o.box.max.y + .15f) continue;
        RayCollision hit = GetRayCollisionBox(ray,o.box);
        if (hit.hit && hit.distance < len) return true;
    }
    return false;
}

static void DrawLimb(Vector3 a, Vector3 b, float radius, Color color) {
    DrawCylinderEx(a,b,radius,radius*.86f,10,color);
    DrawSphere(b,radius*.93f,color);
}

static Vector3 RotateYLocal(Vector3 v, float yaw) {
    float s=sinf(yaw), c=cosf(yaw);
    return {v.x*c+v.z*s, v.y, -v.x*s+v.z*c};
}

static void DrawHuman(const Fighter& f) {
    float t=f.animTime;
    float walk = (f.anim==Anim::Walk) ? sinf(t*10.0f) : 0;
    float punch = (f.anim==Anim::Punch) ? sinf(std::min(t/0.52f,1.0f)*PI) : 0;
    float kick = (f.anim==Anim::Kick) ? sinf(std::min(t/0.72f,1.0f)*PI) : 0;
    float hit = (f.anim==Anim::Hit) ? sinf(std::min(t/.35f,1.0f)*PI) : 0;
    float crouch = f.crouching ? .75f : 0;
    Vector3 root = Vector3Add(f.pos,V3(0,.95f-crouch,0));
    Vector3 fw=Forward(f.yaw), right={fw.z,0,-fw.x};
    auto world=[&](Vector3 local){return Vector3Add(root,RotateYLocal(local,f.yaw));};

    // Hips and torso twist make attacks involve the whole body.
    float twist = punch*.42f - kick*.18f;
    Vector3 hip=world(V3(0,0,0));
    Vector3 chest=world(V3(punch*.10f,1.05f-hit*.18f,punch*.22f));
    DrawCylinderEx(hip,chest,.42f,.55f,12,f.shirt);
    DrawCylinderEx(world(V3(-.34f,.62f,0)),world(V3(.34f,.62f,0)),.16f,.16f,10,f.shirt);

    Color skin = f.enemy ? Color{179,112,80,255} : Color{198,139,98,255};
    Vector3 neck=Vector3Add(chest,V3(0,.25f,0));
    Vector3 head=Vector3Add(neck,V3(0,.34f,0));
    DrawCylinderEx(chest,neck,.20f,.18f,10,skin);
    DrawSphere(head,.34f,skin);
    // short dark hair and beard, matching Fady's reference.
    DrawSphere(Vector3Add(head,V3(0,.16f,-.02f)),.29f,Color{39,27,22,255});
    DrawSphere(Vector3Add(head,Vector3Scale(fw,.27f)),.20f,Color{45,30,25,255});
    DrawSphere(Vector3Add(head,Vector3Add(Vector3Scale(fw,.31f),Vector3Scale(right,.10f))),.035f,BLACK);
    DrawSphere(Vector3Add(head,Vector3Add(Vector3Scale(fw,.31f),Vector3Scale(right,-.10f))),.035f,BLACK);

    Vector3 ls=world(V3(-.55f,1.0f,0)), rs=world(V3(.55f,1.0f,0));
    Vector3 le=world(V3(-.72f,.48f,walk*.20f));
    Vector3 lh=world(V3(-.57f,.05f,-walk*.18f));
    Vector3 re=world(V3(.60f,.50f-punch*.12f,.18f+punch*.58f));
    Vector3 rh=world(V3(.50f,.12f+punch*.25f,.08f+punch*1.25f));
    if (kick>.05f) { le=world(V3(-.58f,.42f,-.12f)); lh=world(V3(-.44f,.06f,-.16f)); }
    DrawLimb(ls,le,.15f,skin); DrawLimb(le,lh,.13f,skin);
    DrawLimb(rs,re,.15f,skin); DrawLimb(re,rh,.13f,skin);

    Vector3 lhip=world(V3(-.24f,0,0)), rhip=world(V3(.24f,0,0));
    Vector3 lk=world(V3(-.27f,-.58f,walk*.23f));
    Vector3 lf=world(V3(-.27f,-1.08f,-walk*.20f));
    Vector3 rk=world(V3(.27f,-.58f,-walk*.23f + kick*.75f));
    Vector3 rf=world(V3(.27f,-1.08f,walk*.20f + kick*1.55f));
    if (f.crouching) { lk=world(V3(-.40f,-.28f,.34f)); lf=world(V3(-.45f,-.63f,-.05f)); rk=world(V3(.40f,-.28f,.34f)); rf=world(V3(.45f,-.63f,-.05f)); }
    Color shorts=f.enemy?Color{75,22,32,255}:Color{20,25,38,255};
    DrawLimb(lhip,lk,.20f,shorts); DrawLimb(lk,lf,.17f,skin);
    DrawLimb(rhip,rk,.20f,shorts); DrawLimb(rk,rf,.17f,skin);
    DrawCube(Vector3Add(lf,Vector3Scale(fw,.11f)),.30f,.15f,.50f,f.accent);
    DrawCube(Vector3Add(rf,Vector3Scale(fw,.11f)),.30f,.15f,.50f,f.accent);

    // Visible equipped weapon in the fighter's hand.
    int w=f.weapon;
    if (w==0 || w==3 || w==6) {
        Color wc = w==6?LIME:(w==3?YELLOW:DARKGRAY);
        DrawCube(Vector3Add(rh,Vector3Scale(fw,.18f)),.16f,.18f,w==6?.70f:.42f,wc);
    } else if (w==2 || w==4) {
        Vector3 tip=Vector3Add(rh,Vector3Scale(fw,w==4?1.45f:1.1f));
        DrawCylinderEx(rh,tip,.055f,.025f,8,w==4?ORANGE:LIGHTGRAY);
        if(w==4) DrawSphere(tip,.14f,RED);
    }
}

static std::vector<Obstacle> MakeObstacles(int round) {
    std::vector<Obstacle> out;
    int count=11+round*2;
    for(int i=0;i<count;i++) {
        float x=(float)GetRandomValue(-420,420)/10.0f;
        float z=(float)GetRandomValue(-300,300)/10.0f;
        if(fabsf(x)<8 && fabsf(z)<7) { x+=14; z+=8; }
        bool low=GetRandomValue(0,99)<45;
        float w=(float)GetRandomValue(25,65)/10.0f;
        float d=(float)GetRandomValue(10,28)/10.0f;
        float h=low?1.05f:(float)GetRandomValue(25,45)/10.0f;
        out.push_back({{{x-w/2,0,z-d/2},{x+w/2,h,z+d/2}},low,low?Color{98,112,119,255}:Color{54,67,81,255}});
    }
    return out;
}

static void DrawWorld(const std::vector<Obstacle>& obs, int round) {
    Color floor = round%2?Color{35,43,55,255}:Color{28,49,44,255};
    DrawPlane(V3(0,0,0),{WORLD_HALF*2,WORLD_HALF*1.45f},floor);
    for(int i=-48;i<=48;i+=4) DrawLine3D(V3((float)i,.01f,-35),V3((float)i,.01f,35),Fade(SKYBLUE,.08f));
    for(int i=-35;i<=35;i+=4) DrawLine3D(V3(-48,.01f,(float)i),V3(48,.01f,(float)i),Fade(SKYBLUE,.08f));
    for(const auto& o:obs) {
        Vector3 size=Vector3Subtract(o.box.max,o.box.min), c=Vector3Scale(Vector3Add(o.box.min,o.box.max),.5f);
        DrawCubeV(c,size,o.color); DrawCubeWiresV(c,size,Fade(o.low?YELLOW:SKYBLUE,.75f));
    }
    // landmarks create multiple visual scenes across the wide arena.
    DrawCube(V3(-34,4,-18),10,8,6,Color{48,42,64,255});
    DrawCube(V3(35,3,19),13,6,8,Color{35,62,68,255});
    for(int i=-4;i<=4;i++) DrawCylinder(V3(i*9.0f,1.7f,-32),1.0f,1.3f,3.4f,12,Color{65,52,45,255});
}

static bool CanMove(Vector3 p, bool crouch, const std::vector<Obstacle>& obs) {
    float h=crouch?1.1f:2.0f;
    BoundingBox me{{p.x-.42f,p.y,p.z-.42f},{p.x+.42f,p.y+h,p.z+.42f}};
    for(const auto& o:obs) if(CheckCollisionBoxes(me,o.box)) return false;
    return fabsf(p.x)<WORLD_HALF-1 && fabsf(p.z)<34;
}

static void Hurt(Fighter& target,float dmg,Vector3 push) {
    if(target.invuln>0) return;
    target.hp=std::max(0.0f,target.hp-dmg);
    target.vel=Vector3Add(target.vel,push);
    target.invuln=.14f; target.anim=Anim::Hit; target.animTime=0;
}

static void FireWeapon(Fighter& a, Fighter& target, std::vector<Projectile>& shots, int w, const std::vector<Obstacle>& obs, bool applyDamage=true) {
    const auto& info=WEAPONS[w];
    if(a.cooldown>0) return;
    a.weapon=w; a.cooldown=info.cooldown;
    Vector3 origin=Vector3Add(a.pos,V3(0,a.crouching?.95f:1.6f,0));
    Vector3 aim=Vector3Add(target.pos,V3(0,target.crouching?.75f:1.35f,0));
    Vector3 dir=Vector3Normalize(Vector3Subtract(aim,origin));
    a.yaw=atan2f(dir.x,dir.z);
    float dist=Vector3Distance(a.pos,target.pos);
    if(w==2 || w==4 || w==5) {
        a.anim=(w==5)?Anim::Kick:Anim::Punch; a.animTime=0;
        if(applyDamage && dist<info.range && !SegmentBlocked(origin,aim,obs,origin.y))
            Hurt(target,info.damage,Vector3Scale(dir,w==5?8.0f:3.0f));
        return;
    }
    float speed=(w==1)?13.0f:(w==6?11.0f:38.0f);
    shots.push_back({origin,Vector3Scale(dir,speed),w==6?.35f:(w==1?.22f:.10f),info.damage,w==6?4.0f:2.6f,w,a.enemy,info.color});
}

static void ResetRound(Fighter& p,Fighter& e,int round,std::vector<Projectile>& shots) {
    p={V3(-6,0,0),{},0,MAX_HP,0,0,0,0,Anim::Idle,true,false,false,BLACK,WHITE};
    e={V3(6,0,1),{},PI,MAX_HP+round*120.0f,0,0,0,std::min(round+1,6),Anim::Idle,true,false,true,Color{98,22,35,255},RED};
    shots.clear();
}

static void DrawBar(int x,int y,int w,int h,float value,Color c,const char* label) {
    DrawRectangleRounded({(float)x,(float)y,(float)w,(float)h},.35f,12,Color{10,13,20,230});
    DrawRectangleRounded({(float)x+5,(float)y+5,(w-10)*Clamp01(value),(float)h-10},.32f,12,c);
    DrawText(label,x+12,y+8,22,WHITE);
}

static const char* KeyName(int i) { static const char* k[]={"1","2","3","4","5","6","7"}; return k[i]; }

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT|FLAG_WINDOW_RESIZABLE|FLAG_VSYNC_HINT);
        ix::initNetSystem();
    InitWindow(SCREEN_W,SCREEN_H,"Fady: Arena Ascension");
    SetTargetFPS(60);

    Screen screen=Screen::MainMenu;
    bool online=false;
    NetworkClient net;
    net.loadConfig("client_config.txt");
    float netSendTimer=0;
    int loadedOnlineRound=0;

    int round=1, unlocked=3;
    GameState state=GameState::Playing;
    Fighter player, enemy;
    std::vector<Projectile> shots;
    std::vector<Obstacle> obstacles=MakeObstacles(round);
    ResetRound(player,enemy,round,shots);
    float notice=5.0f;
    std::string noticeText="ROUND 1: [1] PISTOL  [2] FIREBALL  [3] SWORD";
    float aiTimer=.4f;

    Camera3D cam{}; cam.up={0,1,0}; cam.fovy=54; cam.projection=CAMERA_PERSPECTIVE;

    while(!WindowShouldClose()) {
        float dt=std::min(GetFrameTime(),.033f);
        int sw=GetScreenWidth(), sh=GetScreenHeight();

        if(online) net.pump();
        if(screen==Screen::MainMenu) {
            if(IsKeyPressed(KEY_F)){online=false;screen=Screen::Arena;state=GameState::Playing;}
            if(IsKeyPressed(KEY_O)){online=true;net.connect();screen=Screen::OnlineMenu;}
        } else if(screen==Screen::OnlineMenu) {
            if(IsKeyPressed(KEY_C)&&net.connected){net.createRoom();screen=Screen::WaitingRoom;}
            if(IsKeyPressed(KEY_J)&&net.connected){net.listRooms();screen=Screen::RoomBrowser;}
            if(IsKeyPressed(KEY_ESCAPE)){net.stop();online=false;screen=Screen::MainMenu;}
        } else if(screen==Screen::RoomBrowser) {
            if(IsKeyPressed(KEY_R))net.listRooms();
            for(int i=0;i<(int)net.rooms.size()&&i<9;i++) if(IsKeyPressed(KEY_ONE+i)){net.joinRoom(net.rooms[i].id);screen=Screen::WaitingRoom;}
            if(IsKeyPressed(KEY_ESCAPE))screen=Screen::OnlineMenu;
        } else if(screen==Screen::WaitingRoom) {
            if(net.playing){screen=Screen::Arena;round=net.round;SetRandomSeed(net.obstacleSeed);obstacles=MakeObstacles(round);loadedOnlineRound=round;ResetRound(player,enemy,round,shots);state=GameState::Playing;
                notice=5;noticeText="SERVER UNLOCKED: "+std::string(WEAPONS[net.unlockedWeapon].name)+" ["+KeyName(net.unlockedWeapon)+"]";}
            if(IsKeyPressed(KEY_ESCAPE)){net.stop();online=false;screen=Screen::MainMenu;}
        }

        if(screen!=Screen::Arena) {
            BeginDrawing();ClearBackground(Color{8,12,20,255});
            const char* title="FADY: ARENA ASCENSION";DrawText(title,sw/2-MeasureText(title,52)/2,95,52,GOLD);
            DrawText("Developed by Fady Gamil",sw/2-145,160,22,Fade(WHITE,.75f));
            if(screen==Screen::MainMenu){
                DrawText("[F] PLAY OFFLINE",sw/2-170,300,34,SKYBLUE);DrawText("[O] PLAY ONLINE",sw/2-170,360,34,LIME);
            } else if(screen==Screen::OnlineMenu){
                DrawText(net.connected?"CONNECTED TO SERVER":"CONNECTING...",sw/2-170,245,26,net.connected?LIME:YELLOW);
                DrawText(net.url.c_str(),sw/2-MeasureText(net.url.c_str(),20)/2,282,20,GRAY);
                DrawText("[C] CREATE ROOM",sw/2-170,350,32,SKYBLUE);DrawText("[J] JOIN A ROOM",sw/2-170,408,32,GOLD);
                DrawText("ESC: BACK",35,sh-45,20,GRAY);
            } else if(screen==Screen::RoomBrowser){
                DrawText("AVAILABLE ROOMS",sw/2-150,225,32,WHITE);
                if(net.rooms.empty())DrawText("No waiting rooms. Press R to refresh.",sw/2-225,290,24,GRAY);
                for(int i=0;i<(int)net.rooms.size()&&i<9;i++)DrawText(TextFormat("[%d] ROOM %s   HOST: %s",i+1,net.rooms[i].id.c_str(),net.rooms[i].host.c_str()),sw/2-280,285+i*44,24,SKYBLUE);
                DrawText("R: REFRESH   ESC: BACK",35,sh-45,20,GRAY);
            } else if(screen==Screen::WaitingRoom){
                DrawText("WAITING FOR SECOND PLAYER",sw/2-245,285,32,YELLOW);
                DrawText("ROOM ID",sw/2-75,350,24,WHITE);DrawText(net.roomId.c_str(),sw/2-MeasureText(net.roomId.c_str(),64)/2,386,64,GOLD);
                DrawText("Give this ID to the other player",sw/2-200,470,22,GRAY);
            }
            if(!net.error.empty())DrawText(net.error.c_str(),sw/2-MeasureText(net.error.c_str(),20)/2,sh-90,20,RED);
            EndDrawing();continue;
        }

        std::vector<int> availableWeapons;
        if(online) availableWeapons=net.available;
        else for(int i=0;i<unlocked;i++)availableWeapons.push_back(i);

        if(online) {
            if(net.round!=loadedOnlineRound){SetRandomSeed(net.obstacleSeed);obstacles=MakeObstacles(net.round);loadedOnlineRound=net.round;ResetRound(player,enemy,net.round,shots);}
            if(auto* me=net.me()){player.hp=me->hp;}
            if(auto* other=net.rival()){enemy.pos=V3(other->x,other->y,other->z);enemy.yaw=other->yaw;enemy.hp=other->hp;enemy.crouching=other->crouch;enemy.weapon=other->weapon;enemy.anim=Anim::Walk;}
            round=net.round;
            if(net.roundEnded){state=net.roundWinner==net.playerId?GameState::RoundWon:GameState::Defeated;}
            else if(net.complete)state=GameState::Complete;
            else if(net.playing)state=GameState::Playing;
        }

        if(state==GameState::Playing) {
            player.cooldown=std::max(0.0f,player.cooldown-dt); enemy.cooldown=std::max(0.0f,enemy.cooldown-dt);
            player.invuln=std::max(0.0f,player.invuln-dt); enemy.invuln=std::max(0.0f,enemy.invuln-dt);
            player.animTime+=dt; enemy.animTime+=dt; notice=std::max(0.0f,notice-dt);
            Vector3 move{};
            if(IsKeyDown(KEY_W)) move.z+=1; if(IsKeyDown(KEY_S)) move.z-=1;
            if(IsKeyDown(KEY_A)) move.x-=1; if(IsKeyDown(KEY_D)) move.x+=1;
            player.crouching=IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_C);
            float ml=Vector3Length(move);
            if(ml>.1f) {
                move=Vector3Scale(Vector3Normalize(move),player.crouching?3.1f:6.2f);
                Vector3 np=Vector3Add(player.pos,Vector3Scale(move,dt));
                if(CanMove(np,player.crouching,obstacles)) player.pos=np;
                player.yaw=atan2f(move.x,move.z); player.anim=Anim::Walk;
            } else if(player.animTime>.75f) player.anim=player.crouching?Anim::Crouch:Anim::Idle;
            if(IsKeyPressed(KEY_SPACE)&&player.grounded) {player.vel.y=8.2f;player.grounded=false;player.anim=Anim::Jump;player.animTime=0;}
            player.vel.y-=19.0f*dt; player.pos.y+=player.vel.y*dt;
            if(player.pos.y<=FLOOR_Y){player.pos.y=FLOOR_Y;player.vel.y=0;player.grounded=true;}

            if(IsKeyPressed(KEY_TAB)) {auto it=std::find(availableWeapons.begin(),availableWeapons.end(),player.weapon);player.weapon=availableWeapons[(it==availableWeapons.end()?0:(int)(it-availableWeapons.begin()+1)%availableWeapons.size())];}
            for(int w:availableWeapons) if(IsKeyPressed(WEAPONS[w].key)) player.weapon=w;
            if(IsKeyPressed(KEY_J)) { player.anim=Anim::Punch;player.animTime=0; if(!online&&Vector3Distance(player.pos,enemy.pos)<2.7f) Hurt(enemy,28,Vector3Scale(Forward(player.yaw),2)); }
            if(IsKeyPressed(KEY_K)) { int w=std::find(availableWeapons.begin(),availableWeapons.end(),5)!=availableWeapons.end()?5:player.weapon; if(online)net.attack(w);FireWeapon(player,enemy,shots,w,obstacles,!online);player.anim=Anim::Kick;player.animTime=0; }
            bool automatic=player.weapon==3;
            if((automatic?IsKeyDown(KEY_F):IsKeyPressed(KEY_F)) || IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                if(online)net.attack(player.weapon);
                FireWeapon(player,enemy,shots,player.weapon,obstacles,!online);
            }

            if(online){netSendTimer-=dt;if(netSendTimer<=0){net.input(player.pos.x,player.pos.y,player.pos.z,player.yaw,player.crouching);netSendTimer=.05f;}}

            // Enemy follows through the large arena and attacks with round-appropriate equipment.
            Vector3 toP=Vector3Subtract(player.pos,enemy.pos); toP.y=0; float dist=Vector3Length(toP);
            if(dist>.01f) enemy.yaw=atan2f(toP.x,toP.z);
            aiTimer-=dt;
            if(!online&&dist>7.5f) {
                Vector3 step=Vector3Scale(Vector3Normalize(toP),4.0f*dt);
                Vector3 np=Vector3Add(enemy.pos,step);
                if(CanMove(np,false,obstacles)) enemy.pos=np;
                else { np=Vector3Add(enemy.pos,V3(-step.z,0,step.x)); if(CanMove(np,false,obstacles)) enemy.pos=np; }
                enemy.anim=Anim::Walk;
            }
            if(!online&&aiTimer<=0) {
                int ew=std::min(round+1,6);
                if(dist<WEAPONS[ew].range || ew==0||ew==1||ew==3||ew==6) FireWeapon(enemy,player,shots,ew,obstacles);
                aiTimer=(float)GetRandomValue(28,72)/100.0f + (ew==3?-.2f:0);
            }

            for(auto& s:shots) {
                s.life-=dt; Vector3 old=s.pos; s.pos=Vector3Add(s.pos,Vector3Scale(s.vel,dt));
                bool blocked=false; for(const auto&o:obstacles) if(SphereHitsBox(s.pos,s.radius,o.box)){blocked=true;break;}
                Fighter& target=s.fromEnemy?player:enemy;
                float targetH=target.crouching?1.0f:1.75f;
                if(!blocked && Vector3Distance(s.pos,Vector3Add(target.pos,V3(0,targetH*.55f,0)))<s.radius+.55f) {
                    if(!online)Hurt(target,s.damage,Vector3Scale(Vector3Normalize(s.vel),s.kind==6?11.0f:2.0f)); s.life=-1;
                }
                if(blocked) s.life=-1;
            }
            shots.erase(std::remove_if(shots.begin(),shots.end(),[](const Projectile&s){return s.life<=0;}),shots.end());
            if(!online&&enemy.hp<=0) { state=GameState::RoundWon; notice=0; }
            if(!online&&player.hp<=0) state=GameState::Defeated;
        } else if(!online&&state==GameState::RoundWon && IsKeyPressed(KEY_ENTER)) {
            if(round>=5) state=GameState::Complete;
            else { round++; unlocked=std::min(2+round,7); obstacles=MakeObstacles(round); ResetRound(player,enemy,round,shots); state=GameState::Playing;
                notice=5; noticeText="UNLOCKED: ["+std::string(KeyName(unlocked-1))+"] "+WEAPONS[unlocked-1].name; }
        } else if(!online&&state==GameState::Defeated && IsKeyPressed(KEY_R)) {
            obstacles=MakeObstacles(round); ResetRound(player,enemy,round,shots); state=GameState::Playing;
        }

        cam.target=Vector3Add(player.pos,V3(0,1.1f,0));
        cam.position=Vector3Add(player.pos,V3(-9.5f,7.2f,-11.5f));

        BeginDrawing(); ClearBackground(Color{8,12,20,255});
        BeginMode3D(cam);
        DrawWorld(obstacles,round);
        for(const auto&s:shots) {
            DrawSphere(s.pos,s.radius,s.color);
            if(s.kind==1||s.kind==6) DrawSphereWires(s.pos,s.radius*1.8f,8,8,Fade(s.color,.5f));
        }
        DrawHuman(player); DrawHuman(enemy);
        EndMode3D();

        const char* myName=online?net.playerName.c_str():"FADY";
        const char* rivalName=(online&&net.rival())?net.rival()->name.c_str():"RIVAL";
        DrawBar(28,24,sw/2-80,54,player.hp/MAX_HP,Color{35,210,115,255},TextFormat("%s  %.0f / %.0f",myName,player.hp,MAX_HP));
        float enemyMax=online?MAX_HP:MAX_HP+round*120.0f;
        DrawBar(sw/2+52,24,sw/2-80,54,enemy.hp/enemyMax,Color{235,67,76,255},TextFormat("%s  %.0f",rivalName,enemy.hp));
        DrawText(TextFormat("ROUND %d / 5",round),sw/2-72,91,27,WHITE);

        int cardW=145, total=(int)availableWeapons.size()*cardW, start=sw/2-total/2;
        for(int card=0;card<(int)availableWeapons.size();card++) {
            int i=availableWeapons[card]; Rectangle r{(float)(start+card*cardW),126,(float)cardW-8,53};
            DrawRectangleRounded(r,.20f,8,i==player.weapon?Fade(WEAPONS[i].color,.72f):Color{16,21,31,225});
            DrawRectangleRoundedLinesEx(r,.20f,8,i==player.weapon?3.0f:1.0f,i==player.weapon?WHITE:Fade(WHITE,.25f));
            DrawText(TextFormat("[%s]",KeyName(i)),(int)r.x+8,(int)r.y+7,18,WHITE);
            DrawText(WEAPONS[i].name,(int)r.x+8,(int)r.y+29,14,WHITE);
        }
        DrawText("MOVE: WASD   JUMP: SPACE   CROUCH: CTRL/C   ATTACK: F or MOUSE   PUNCH: J   KICK: K",24,sh-40,18,Fade(WHITE,.80f));
        DrawText("Developed by Fady Gamil",sw-255,sh-38,18,GOLD);
        if(notice>0) {
            int tw=MeasureText(noticeText.c_str(),30); Rectangle b{(float)(sw/2-tw/2-28),205,(float)(tw+56),68};
            DrawRectangleRounded(b,.25f,12,Color{7,10,17,235}); DrawRectangleRoundedLinesEx(b,.25f,12,2,GOLD);
            DrawText(noticeText.c_str(),sw/2-tw/2,224,30,WHITE);
        }
        bool enemyVisible=Vector3Distance(player.pos,enemy.pos)<25.0f;
        if(!enemyVisible && state==GameState::Playing) DrawText("RIVAL OUT OF VIEW - FOLLOW THE RED MARKER",sw/2-250,195,22,RED);
        if(state!=GameState::Playing) {
            DrawRectangle(0,0,sw,sh,Fade(BLACK,.76f));
            const char* title=state==GameState::RoundWon?"ROUND WON":(state==GameState::Defeated?"YOU WERE DEFEATED":"ARENA CHAMPION");
            Color tc=state==GameState::Defeated?RED:GOLD; int fs=64, tw=MeasureText(title,fs);
            DrawText(title,sw/2-tw/2,sh/2-100,fs,tc);
            if(state==GameState::RoundWon) DrawText(online?"Server is preparing the next round...":(round==5?"Press ENTER to claim victory":"Press ENTER to unlock the next arena"),sw/2-250,sh/2,28,WHITE);
            else if(state==GameState::Defeated) DrawText(online?"Server is preparing the next round...":"Press R to retry this round",sw/2-185,sh/2,28,WHITE);
            else { DrawText(online?(net.championId==net.playerId?"YOU ARE THE ONLINE CHAMPION":"MATCH COMPLETE"):"ALL FIVE ROUNDS COMPLETE",sw/2-205,sh/2,30,WHITE); DrawText("Developed by Fady Gamil",sw/2-180,sh/2+58,30,GOLD); }
        }
        EndDrawing();
    }
    CloseWindow();
    ix::uninitNetSystem();

    return 0;
}
