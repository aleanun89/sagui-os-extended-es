#include "game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *const mob_names[MOB_COUNT] = {
    "Zorro", "Jabali", "Liebre", "Aldeano", "Umbrio", "Coloso", "Candil errante"
};

const Recipe recipes[] = {
    {PLANK,4,LOG,1,AIR,0,AIR}, {RED_PLANK,4,BIRCH,1,AIR,0,AIR},
    {PICK,1,STONE,3,PLANK,2,AIR}, {AXE,1,STONE,3,PLANK,1,AIR},
    {SHOVEL,1,STONE,1,PLANK,2,AIR}, {BLADE,1,IRON,2,PLANK,1,AIR},
    {COBBLE,2,STONE,2,AIR,0,AIR}, {WORKBENCH,1,PLANK,4,AIR,0,AIR},
    {BRICK,4,CLAY,4,COAL,1,AIR}, {GLASS,3,SAND,3,COAL,1,AIR},
    {LANTERN,2,IRON,1,COAL,1,AIR}, {CLOTH,2,BERRIES,2,LEAVES,2,AIR},
    // Ideas tomadas de juegos libres tipo Luanti (D449), reescritas aqui: madera de pino, ladrillos,
    // horno (hay que llevarlo en la mochila para fundir) y herramientas de hierro y de diamante.
    {PINE_PLANK,4,PINE_LOG,1,AIR,0,AIR}, {STONE_BRICK,4,STONE,4,AIR,0,AIR},
    {SANDSTONE,1,SAND,4,AIR,0,AIR}, {FURNACE,1,COBBLE,8,AIR,0,AIR},
    {INGOT_IRON,1,IRON,1,COAL,1,FURNACE}, {INGOT_COPPER,1,COPPER_ORE,1,COAL,1,FURNACE},
    {PICK_IRON,1,INGOT_IRON,3,PLANK,2,AIR}, {AXE_IRON,1,INGOT_IRON,3,PLANK,1,AIR},
    {BLADE_IRON,1,INGOT_IRON,2,PLANK,1,AIR},
    {PICK_GEM,1,GEM,3,PLANK,2,AIR}, {AXE_GEM,1,GEM,3,PLANK,1,AIR}, {BLADE_GEM,1,GEM,2,PLANK,1,AIR},
    {LANTERN,4,INGOT_COPPER,1,COAL,1,AIR}
};
const size_t recipe_count = sizeof(recipes)/sizeof(recipes[0]);

static const float body_height[MOB_COUNT] = {.68f,.83f,.52f,1.72f,1.70f,1.95f,.85f};
static const float body_radius[MOB_COUNT] = {.27f,.37f,.22f,.29f,.29f,.43f,.27f};
static const int mob_health[MOB_COUNT] = {4,6,3,20,6,13,5};

static float distance2(Vec3 a, Vec3 b)
{
    const float x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;
    return x*x+y*y+z*z;
}

static Vec3 plus_scaled(Vec3 p, Vec3 direction, float amount)
{
    return (Vec3){p.x + amount*direction.x, p.y + amount*direction.y, p.z + amount*direction.z};
}

Vec3 view_direction(const Game *g)
{
    float c=cosf(g->pitch);
    return (Vec3){sinf(g->yaw)*c,sinf(g->pitch),-cosf(g->yaw)*c};
}

static Vec3 eyes(const Game *g)
{
    return (Vec3){g->pos.x,g->pos.y+1.59f,g->pos.z};
}

float daylight(const Game *g)
{
    return clampf(.43f+.70f*sinf(g->time*2*PI),.14f,1.0f);
}

void notify(Game *g, const char *message)
{
    snprintf(g->notice,sizeof(g->notice),"%s",message);
    g->notice_time=3.6f;
}

void give(Game *g, int item, unsigned quantity)
{
    if (item<=AIR || item>=ITEM_COUNT) return;
    unsigned total = g->bag[item]+quantity;
    g->bag[item]=(uint16_t)(total>999u ? 999u : total);
    for (int i=0; i<SLOTS; ++i) if (g->slots[i]==item) return;
    for (int i=0; i<SLOTS; ++i) {
        if (g->slots[i]==AIR) {
            g->slots[i]=(uint8_t)item;
            return;
        }
    }
}

bool craft(Game *g, size_t i)
{
    if (i>=recipe_count) return false;
    const Recipe *r=&recipes[i];
    if (!g->creative && r->need!=AIR && !g->bag[r->need]) {
        notify(g,"Hace falta tener un horno en la mochila");
        return false;
    }
    if (!g->creative && (g->bag[r->first]<r->count_first ||
        g->bag[r->second]<r->count_second || g->bag[r->output]>999-r->quantity)) {
        notify(g,"Faltan materiales o espacio");
        return false;
    }
    if (!g->creative) {
        g->bag[r->first]-=(uint16_t)r->count_first;
        g->bag[r->second]-=(uint16_t)r->count_second;
    }
    give(g,r->output,(unsigned)r->quantity);
    g->sound=SND_CRAFT;
    notify(g,"Fabricado");
    return true;
}

bool body_blocked(World *w, Vec3 p, float radius, float height)
{
    const int x0=(int)floorf(p.x-radius), x1=(int)floorf(p.x+radius);
    const int z0=(int)floorf(p.z-radius), z1=(int)floorf(p.z+radius);
    const int y0=(int)floorf(p.y+.002f), y1=(int)floorf(p.y+height-.002f);
    for (int y=y0; y<=y1; ++y) for (int z=z0; z<=z1; ++z)
        for (int x=x0; x<=x1; ++x)
            if (solid_at(w,x,y,z)) return true;
    return false;
}

static bool move_axis(World *w, Vec3 *p, int axis, float amount, float r, float h)
{
    if (amount==0) return true;
    Vec3 target=*p;
    float *coordinate = axis==0 ? &target.x : (axis==1 ? &target.y : &target.z);
    *coordinate+=amount;
    if (!body_blocked(w,target,r,h)) {
        *p=target;
        return true;
    }
    // Advance to the surface to avoid hovering above the ground after a fall.
    float lo=0,hi=amount;
    for (int i=0; i<8; ++i) {
        const float mid=(lo+hi)*.5f;
        target=*p;
        *coordinate+=mid;
        if (body_blocked(w,target,r,h)) hi=mid;
        else lo=mid;
    }
    *coordinate = (axis==0 ? p->x : (axis==1 ? p->y : p->z)) + lo;
    *p=target;
    return false;
}

static uint32_t next_rand(Game *g)
{
    g->random = g->random*UINT32_C(1664525) + UINT32_C(1013904223);
    return g->random;
}

static float unit(Game *g) { return (float)(next_rand(g)>>8)/16777216.0f; }

static bool spawn(Game *g, Mob *m, Species kind, int x, int z)
{
    if (!inside(&g->world,x-1,0,z-1) || !inside(&g->world,x+1,0,z+1)) return false;
    const int y=surface_at(&g->world,x,z);
    const int floor=block_at(&g->world,x,y,z);
    if (floor==WATER || floor==LEAVES || floor==AUTUMN || floor==LANTERN ||
        floor==BRICK || floor==LOG || floor==PLANK || floor==RED_PLANK || floor==GLASS ||
        floor==NEEDLES || floor==PINE_LOG || floor==PINE_PLANK || floor==LAVA) return false;
    *m=(Mob){.kind=kind,.hp=mob_health[kind],.active=true,
        .pos={(float)x+.5f,(float)y+1.02f,(float)z+.5f},
        .yaw=unit(g)*2*PI, .wander=1+unit(g)*4, .phase=unit(g)*2*PI};
    return true;
}

void refresh_wildlife(Game *g,bool force)
{
    const int zone_x=(int)floorf(g->pos.x/32),zone_z=(int)floorf(g->pos.z/32);
    if (!force && zone_x==g->wildlife_x && zone_z==g->wildlife_z) return;
    g->wildlife_x=zone_x;
    g->wildlife_z=zone_z;
    const int near_x=(int)floorf(g->pos.x),near_z=(int)floorf(g->pos.z);
    for (int i=10; i<48; ++i) {
        Mob *m=&g->mobs[i];
        const float dx=m->pos.x-g->pos.x,dz=m->pos.z-g->pos.z;
        if (m->active && !force && dx*dx+dz*dz<67*67) continue;
        Species kind=(Species)(i%MOB_COUNT);
        if (kind==VILLAGER) kind=HARE;
        *m=(Mob){.kind=kind};
        for (int attempt=0; attempt<18; ++attempt) {
            uint32_t hash=terrain_hash(g->world.seed^(uint32_t)(i*29+attempt),zone_x,i,zone_z);
            const int x=near_x+(int)(hash%103u)-51;
            const int z=near_z+(int)((hash>>16)%103u)-51;
            const int rx=x-near_x,rz=z-near_z;
            if (rx*rx+rz*rz<19*19 || rx*rx+rz*rz>56*56) continue;
            if (abs(x-80)<17 && abs(z-82)<18) continue;
            if (spawn(g,m,kind,x,z)) break;
        }
    }
}

void new_game(Game *g, uint32_t seed,WorldSize size)
{
    close_world(&g->world);
    memset(g,0,sizeof(*g));
    generate(&g->world,seed,size);
    g->random=seed^UINT32_C(0xf2a39b91);
    g->pos=g->world.start;
    g->pitch=-.065f;
    g->health=20;
    g->time=.17f;
    g->mob_target=-1;
    give(g,PICK,1);
    give(g,AXE,1);
    give(g,SHOVEL,1);
    give(g,PLANK,16);
    give(g,COBBLE,12);
    give(g,BERRIES,4);
    static const int positions[10][2]={{74,89},{87,93},{76,85},{79,83},
        {66,90},{94,93},{68,107},{77,80},{87,78},{81,88}};
    static const Species kinds[10]={FOX,BOAR,HARE,VILLAGER,SHADE,GOLEM,WISP,VILLAGER,VILLAGER,HARE};
    for (int i=0; i<10; ++i) spawn(g,&g->mobs[i],kinds[i],positions[i][0],positions[i][1]);
    refresh_wildlife(g,true);
    notify(g,"Aldea de Bruma al norte. E abre la mochila.");
    aim(g);
}

static void damage_player(Game *g, int damage)
{
    if (g->creative || g->hurt>.001f) return;
    g->health-=damage;
    g->hurt=.75f;
    g->sound=SND_HURT;
    if (g->health>0) return;
    g->health=20;
    g->pos=g->world.start;
    for (int i=0; i<g->world.size.height && body_blocked(&g->world,g->pos,.29f,1.75f); ++i)
        g->pos.y+=1;
    g->vy=0;
    g->yaw=0;
    g->pitch=-.065f;
    notify(g,"Has regresado a la aldea");
}

static void walk_player(Game *g, Input input, float dt)
{
    const int x=(int)floorf(g->pos.x), z=(int)floorf(g->pos.z);
    const int at_chest=block_at(&g->world,x,(int)floorf(g->pos.y+.55f),z);
    const int at_feet=block_at(&g->world,x,(int)floorf(g->pos.y+.2f),z);
    g->swimming=at_chest==WATER || at_chest==LAVA;
    // La lava quema: dano cada vez que se acaba la proteccion tras el golpe anterior.
    if (at_chest==LAVA || at_feet==LAVA) damage_player(g,4);
    float speed=g->swimming ? 2.4f : (input.sprint ? 7.1f : 4.5f);
    float f=input.forward,s=input.side;
    const float length=sqrtf(f*f+s*s);
    if (length>1) { f/=length; s/=length; }
    const float a=sinf(g->yaw),b=cosf(g->yaw);
    move_axis(&g->world,&g->pos,0,(f*a+s*b)*speed*dt,.29f,1.75f);
    move_axis(&g->world,&g->pos,2,(-f*b+s*a)*speed*dt,.29f,1.75f);
    if (g->swimming) g->vy=input.jump ? 3.8f : -1.8f;
    else if (input.jump && g->grounded) g->vy=7.0f;
    else g->vy=fmaxf(-25,g->vy-18.5f*dt);
    g->grounded=false;
    if (!move_axis(&g->world,&g->pos,1,g->vy*dt,.29f,1.75f)) {
        const float impact=g->vy;
        g->grounded=impact<0;
        g->vy=0;
        if (impact < -13.0f) damage_player(g,(int)((-impact-11)*.65f));
    }
    g->step_clock-=dt;
    if (g->grounded && length>.05f && g->step_clock<=0) {
        g->sound=SND_STEP;
        g->step_clock=input.sprint?.28f:.43f;
    }
}

static void launch(Game *g, const Mob *m)
{
    for (int i=0; i<MAX_BOLTS; ++i) {
        Bolt *b=&g->bolts[i];
        if (b->lifetime>0) continue;
        b->pos=(Vec3){m->pos.x,m->pos.y+.53f,m->pos.z};
        Vec3 p=eyes(g);
        const float scale=8.0f/fmaxf(.01f,sqrtf(distance2(p,b->pos)));
        b->velocity=(Vec3){(p.x-b->pos.x)*scale,(p.y-b->pos.y)*scale,(p.z-b->pos.z)*scale};
        b->lifetime=2.5f;
        return;
    }
}

static void move_mobs(Game *g, float dt)
{
    refresh_wildlife(g,false);
    const bool dark=daylight(g)<.64f;
    for (int i=0; i<MAX_MOBS; ++i) {
        Mob *m=&g->mobs[i];
        if (!m->active || distance2(m->pos,g->pos)>65*65) continue;
        const Species k=m->kind;
        const bool hostile=k>=SHADE;
        m->cooldown=fmaxf(m->cooldown-dt,0);
        m->hurt=fmaxf(m->hurt-dt,0);
        m->flee=fmaxf(m->flee-dt,0);
        const float dx=g->pos.x-m->pos.x,dz=g->pos.z-m->pos.z;
        const float d=sqrtf(dx*dx+dz*dz);
        const bool hunt=hostile && !g->creative && d<((k==WISP || dark)?14:7)
            && fabsf(g->pos.y-m->pos.y)<5;
        const bool flee=k<VILLAGER && (m->flee>0 || (k==HARE && d<3));
        float speed=.60f;
        bool heading=false;
        if (hunt || flee) {
            m->yaw=atan2f(dx,-dz)+(flee?PI:0);
            speed=flee ? 3.4f : (k==GOLEM ? 1.3f : 2.5f);
            heading=true;
        } else {
            m->wander-=dt;
            if (m->wander<=0) {
                m->yaw+=(unit(g)-.5f)*3.8f;
                m->wander=1.2f+unit(g)*5.0f;
            }
            heading=m->wander>1.3f;
            if (k==VILLAGER && (m->pos.x<71 || m->pos.x>89 || m->pos.z<76 || m->pos.z>91)) {
                m->yaw=atan2f(79.5f-m->pos.x,m->pos.z-83.0f);
                heading=true;
            }
        }
        if (hunt && k==WISP && d<10 && d>1.4f) {
            speed=d>5?1.3f:0;
            Vec3 p={m->pos.x,m->pos.y+.53f,m->pos.z};
            Vec3 target=eyes(g);
            const float len=sqrtf(distance2(p,target));
            if (m->cooldown<=0 && len>.1f) {
                Vec3 direction={(target.x-p.x)/len,(target.y-p.y)/len,(target.z-p.z)/len};
                if (!raycast(&g->world,p,direction,len).found) {
                    launch(g,m);
                    m->cooldown=2.4f;
                }
            }
        }
        if (hunt && d<1.24f && fabsf(m->pos.y-g->pos.y)<1.6f) {
            heading=false;
            if (m->cooldown<=0) {
                damage_player(g,k==GOLEM?5:2);
                m->cooldown=k==GOLEM?1.4f:.9f;
            }
        }
        const float r=body_radius[k],h=body_height[k];
        if (heading && speed>0) {
            const float vx=sinf(m->yaw)*speed*dt, vz=-cosf(m->yaw)*speed*dt;
            const bool a=move_axis(&g->world,&m->pos,0,vx,r,h);
            const bool b=move_axis(&g->world,&m->pos,2,vz,r,h);
            if ((!a || !b) && m->vy==0) {
                Vec3 up={m->pos.x+vx*3,m->pos.y+1.05f,m->pos.z+vz*3};
                if (!body_blocked(&g->world,up,r,h)) m->vy=6.7f;
                else if (!hunt) m->wander=0;
            }
            m->phase+=dt*speed*5.0f;
        }
        const int mx=(int)floorf(m->pos.x),mz=(int)floorf(m->pos.z);
        const bool wet=block_at(&g->world,mx,(int)(m->pos.y+.25f),mz)==WATER;
        if (wet) {
            const bool covered=block_at(&g->world,mx,(int)(m->pos.y+.55f),mz)==WATER;
            m->vy=covered?2.0f:0;
        } else m->vy=fmaxf(-16,m->vy-17*dt);
        if (!move_axis(&g->world,&m->pos,1,m->vy*dt,r,h)) m->vy=0;
    }
    for (int i=0; i<MAX_BOLTS; ++i) {
        Bolt *b=&g->bolts[i];
        if (b->lifetime<=0) continue;
        b->lifetime-=dt;
        b->pos=plus_scaled(b->pos,b->velocity,dt);
        if (solid_at(&g->world,(int)floorf(b->pos.x),(int)floorf(b->pos.y),(int)floorf(b->pos.z))) b->lifetime=0;
        else if (distance2(b->pos,(Vec3){g->pos.x,g->pos.y+1.1f,g->pos.z})<.55f) {
            damage_player(g,3);
            b->lifetime=0;
        }
    }
    g->respawn_clock+=dt;
    if (g->respawn_clock>10) {
        g->respawn_clock=0;
        for (int i=10; i<48; ++i) {
            if (g->mobs[i].active) continue;
            Species k=g->mobs[i].kind;
            if (k==VILLAGER) k=HARE;
            for (int j=0; j<8; ++j) {
                int x=(int)floorf(g->pos.x)+(int)(next_rand(g)%71u)-35;
                int z=(int)floorf(g->pos.z)+(int)(next_rand(g)%71u)-35;
                const float dx=(float)x-g->pos.x,dz=(float)z-g->pos.z;
                if (dx*dx+dz*dz>20*20 && spawn(g,&g->mobs[i],k,x,z)) break;
            }
            break;
        }
    }
}

static float intersect_mob(Vec3 p, Vec3 d, const Mob *m)
{
    const float r=body_radius[m->kind]+.06f;
    const float lo[]={m->pos.x-r, m->pos.y, m->pos.z-r};
    const float hi[]={m->pos.x+r, m->pos.y+body_height[m->kind]+.1f, m->pos.z+r};
    const float v[]={p.x,p.y,p.z}, w[]={d.x,d.y,d.z};
    float near=0,far=6;
    for (int i=0; i<3; ++i) {
        if (fabsf(w[i])<1.e-6f) {
            if (v[i]<lo[i] || v[i]>hi[i]) return INFINITY;
            continue;
        }
        const float a=(lo[i]-v[i])/w[i],b=(hi[i]-v[i])/w[i];
        near=fmaxf(near,fminf(a,b));
        far=fminf(far,fmaxf(a,b));
        if (near>far) return INFINITY;
    }
    return near;
}

void aim(Game *g)
{
    const Vec3 p=eyes(g),d=view_direction(g);
    g->aimed=raycast(&g->world,p,d,6);
    float nearest=g->aimed.found ? g->aimed.distance : 5.1f;
    g->mob_target=-1;
    for (int i=0; i<MAX_MOBS; ++i) {
        if (!g->mobs[i].active) continue;
        const float hit=intersect_mob(p,d,&g->mobs[i]);
        if (hit<nearest) {
            nearest=hit;
            g->mob_target=i;
        }
    }
}

// Nivel de herramienta necesario para sacar el bloque (1: cualquiera; 2: pico de hierro o mejor).
static int required_tier(int block)
{
    return block==DIAMOND_ORE || block==OBSIDIAN ? 2 : 1;
}

static int best_tool(int block)
{
    switch (block) {
    case LOG: case BIRCH: case PLANK: case RED_PLANK: case WORKBENCH:
    case PINE_LOG: case PINE_PLANK:
        return AXE;
    case GRASS: case DIRT: case SAND: case RED_SAND: case GRAVEL: case CLAY: case SNOW:
        return SHOVEL;
    case STONE: case BRICK: case COBBLE: case MOSS: case COAL: case IRON:
    case GOLD: case AMBER: case ICE: case COPPER_ORE: case DIAMOND_ORE: case OBSIDIAN:
    case SANDSTONE: case STONE_BRICK: case FURNACE:
        return PICK;
    default: return AIR;
    }
}

static bool mob_occupies(const Mob *m, int x, int y, int z)
{
    const float r=body_radius[m->kind];
    return m->active && x+1>m->pos.x-r && x<m->pos.x+r &&
        z+1>m->pos.z-r && z<m->pos.z+r && y+1>m->pos.y && y<m->pos.y+body_height[m->kind];
}

static void place_or_eat(Game *g, int item)
{
    if (item<=AIR || (!g->creative && !g->bag[item])) return;
    if (item==BERRIES) {
        if (g->health<20) {
            g->health=g->health>16?20:g->health+4;
            if (!g->creative) --g->bag[BERRIES];
            g->sound=SND_CRAFT;
            notify(g,"Has recuperado salud");
        }
        return;
    }
    Hit h=g->aimed;
    if (item>=BLOCK_COUNT || item==BEDROCK || !h.found || !h.adjacent ||
        g->mob_target>=0 || !inside(&g->world,h.px,h.py,h.pz) || h.py==0) return;
    const int prev=block_at(&g->world,h.px,h.py,h.pz);
    if (prev!=AIR && prev!=WATER && prev!=LAVA) return;
    for (int i=0; i<MAX_MOBS; ++i)
        if (mob_occupies(&g->mobs[i],h.px,h.py,h.pz)) return;
    const Vec3 p=g->pos;
    const bool blocked=blocks[item].solid &&
        h.px+1>p.x-.29f && h.px<=p.x+.29f && h.pz+1>p.z-.29f && h.pz<=p.z+.29f &&
        h.py+1>p.y+.002f && h.py<=p.y+1.748f;
    if (!blocked && set_block(&g->world,h.px,h.py,h.pz,item)) {
        if (!g->creative) --g->bag[item];
        g->sound=SND_PLACE;
    }
}

static void trade(Game *g)
{
    if (g->mob_target<0 || g->mobs[g->mob_target].kind!=VILLAGER) return;
    if (g->bag[AMBER] && g->bag[LANTERN]<=995) {
        --g->bag[AMBER];
        give(g,LANTERN,4);
        g->sound=SND_CRAFT;
        notify(g,"Trato: 1 ambar por 4 faroles");
    } else if (g->bag[PLANK]>=3 && g->bag[BERRIES]<=997) {
        g->bag[PLANK]-=3;
        give(g,BERRIES,2);
        g->sound=SND_CRAFT;
        notify(g,"Trato: 3 tablas por 2 frutas");
    } else notify(g,"Aldeano: busco 3 tablas o 1 ambar");
}

// Gasta un uso de la herramienta; al llegar a su vida util se rompe y se pierde una unidad.
static void wear_tool(Game *g, int item)
{
    const int life=tool_life(item);
    if (g->creative || !life) return;
    if (++g->wear[item]<life) return;
    g->wear[item]=0;
    if (g->bag[item]) --g->bag[item];
    char text[64];
    snprintf(text,sizeof(text),"Se ha roto: %s",item_name(item));
    notify(g,text);
    g->sound=SND_HURT;
}

static void attack_mob(Game *g, int item)
{
    Mob *m=&g->mobs[g->mob_target];
    if (m->kind==VILLAGER) return;
    if (distance2(m->pos,g->pos)>3.8f*3.8f) return;
    const int tier=tool_tier(item);
    int damage=tool_kind(item)==BLADE ? 3+2*tier : (tool_kind(item)==AXE ? 2+tier : 2);
    wear_tool(g,item);
    m->hp-=damage;
    m->hurt=.25f;
    m->flee=3;
    g->action_delay=.36f;
    g->sound=SND_HIT;
    if (m->hp<=0) {
        if (m->kind<VILLAGER) give(g,BERRIES,m->kind==BOAR?2u:1u);
        if (m->kind==SHADE) give(g,COAL,2);
        if (m->kind==GOLEM) give(g,IRON,2);
        if (m->kind==WISP) give(g,AMBER,1);
        m->active=false;
    }
}

static void handle_actions(Game *g, Input in, float dt)
{
    int item=g->slots[g->selected];
    if (!g->creative && g->bag[item]==0) item=AIR;
    if (in.place) place_or_eat(g,item);
    if (in.interact) trade(g);
    if (g->mob_target>=0) {
        g->mining=0;
        if (in.attack && g->action_delay<=0) attack_mob(g,item);
        return;
    }
    Hit h=g->aimed;
    if (!in.mine || !h.found) { g->mining=0; return; }
    const int id=block_at(&g->world,h.x,h.y,h.z);
    if (!blocks[id].hardness) { g->mining=0; return; }
    if (g->mined.x!=h.x || g->mined.y!=h.y || g->mined.z!=h.z) g->mining=0;
    g->mined=h;
    const int preferred=best_tool(id);
    const bool right=preferred!=AIR && tool_kind(item)==preferred;
    if (!g->creative && tool_tier(item)*right<required_tier(id) && required_tier(id)>1) {
        g->mining=0;
        if (g->notice_time<=0) notify(g,"Hace falta un pico de hierro o mejor");
        return;
    }
    static const float speed[4]={1.0f,3.4f,5.0f,7.5f};
    const float strength=right ? speed[tool_tier(item)] : 1.0f;
    g->mining=g->creative?1.0f:g->mining+dt*strength/blocks[id].hardness;
    if (g->mining<1) return;
    if (set_block(&g->world,h.x,h.y,h.z,AIR)) {
        give(g,id==DIAMOND_ORE ? GEM : id,1);
        if (right) wear_tool(g,item);
        if ((id==LEAVES || id==AUTUMN) && next_rand(g)%3u==0) give(g,BERRIES,1);
        g->sound=SND_BREAK;
    }
    g->mining=0;
}

void tick(Game *g, Input in, float dt)
{
    g->notice_time=fmaxf(0,g->notice_time-dt);
    g->hurt=fmaxf(0,g->hurt-dt);
    g->action_delay=fmaxf(0,g->action_delay-dt);
    g->time+=dt/300.0f;
    if (g->time>=1) g->time-=1;
    if (g->time<0) g->time=0;
    walk_player(g,in,dt);
    move_mobs(g,dt);
    aim(g);
    handle_actions(g,in,dt);
    aim(g);
}
