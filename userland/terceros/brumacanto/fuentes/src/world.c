#include "game.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define B(name, top, side, bottom, hardness, opacity, solid) \
    {name, top, side, bottom, hardness, opacity, solid}
const Block blocks[BLOCK_COUNT] = {
    B("Aire",0,0,0,0,0,false),
    B("Cesped",0,1,2,.38f,255,true),
    B("Tierra",2,2,2,.34f,255,true),
    B("Piedra",3,3,3,1.6f,255,true),
    B("Arena",4,4,4,.30f,255,true),
    B("Arena roja",5,5,5,.30f,255,true),
    B("Grava",6,6,6,.44f,255,true),
    B("Arcilla",7,7,7,.50f,255,true),
    B("Tronco",9,8,9,1.1f,255,true),
    B("Abedul",11,10,11,1.1f,255,true),
    B("Hojas",12,12,12,.24f,255,true),
    B("Hojas otonales",13,13,13,.24f,255,true),
    B("Tablas",14,14,14,.72f,255,true),
    B("Tablas rojas",15,15,15,.72f,255,true),
    B("Ladrillo",16,16,16,1.3f,255,true),
    B("Guijarros",17,17,17,1.25f,255,true),
    B("Roca musgosa",18,18,18,1.25f,255,true),
    B("Cristal",19,19,19,.36f,220,true),
    B("Agua",20,20,20,0,153,false),
    B("Nieve",21,21,21,.25f,255,true),
    B("Hielo",22,22,22,.54f,255,true),
    B("Carbon",23,23,23,1.9f,255,true),
    B("Hierro",24,24,24,2.3f,255,true),
    B("Oro",25,25,25,2.6f,255,true),
    B("Ambar",26,26,26,2.8f,255,true),
    B("Tela azul",27,27,27,.43f,255,true),
    B("Banco de trabajo",28,14,14,.8f,255,true),
    B("Farol",29,29,29,.5f,255,true),
    B("Roca madre",30,30,30,0,255,true),
    B("Cobre",42,42,42,2.1f,255,true),
    B("Diamante",43,43,43,2.8f,255,true),
    B("Obsidiana",44,44,44,3.6f,255,true),
    B("Pino",46,45,46,1.1f,255,true),
    B("Tablas de pino",47,47,47,.72f,255,true),
    B("Agujas",48,48,48,.24f,255,true),
    B("Arenisca",49,49,49,.85f,255,true),
    B("Ladrillo de piedra",50,50,50,1.4f,255,true),
    B("Horno",50,51,50,1.7f,255,true),
    B("Lava",52,52,52,0,250,false)
};
#undef B

const int item_icons[ITEM_COUNT] = {
    -1,0,2,3,4,5,6,7,9,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,
    42,43,44,45,47,48,49,50,51,52,
    31,32,33,34,35,
    54,55,56,57,58,59,60,61,62
};

int tool_kind(int item)
{
    switch (item) {
    case PICK: case PICK_IRON: case PICK_GEM: return PICK;
    case AXE: case AXE_IRON: case AXE_GEM: return AXE;
    case SHOVEL: return SHOVEL;
    case BLADE: case BLADE_IRON: case BLADE_GEM: return BLADE;
    default: return 0;
    }
}

int tool_tier(int item)
{
    switch (item) {
    case PICK: case AXE: case SHOVEL: case BLADE: return 1;
    case PICK_IRON: case AXE_IRON: case BLADE_IRON: return 2;
    case PICK_GEM: case AXE_GEM: case BLADE_GEM: return 3;
    default: return 0;
    }
}

int tool_life(int item)
{
    static const int life[4]={0,90,260,900};
    return life[tool_tier(item)];
}

/* Numero de objeto de una partida antigua (versiones 1 y 2) en la numeracion actual. */
int old_item_id(int old)
{
    return old<OLD_BLOCK_COUNT ? old : old-OLD_BLOCK_COUNT+BLOCK_COUNT;
}

const char *item_name(int item)
{
    static const char *const extra[] = {"Pico", "Hacha", "Pala", "Espada", "Fruta",
        "Lingote de cobre", "Lingote de hierro", "Diamante",
        "Pico de hierro", "Hacha de hierro", "Espada de hierro",
        "Pico de diamante", "Hacha de diamante", "Espada de diamante"};
    if (item < 0 || item >= ITEM_COUNT) return "?";
    return item < BLOCK_COUNT ? blocks[item].name : extra[item - BLOCK_COUNT];
}

float clampf(float x, float min, float max)
{
    return x < min ? min : (x > max ? max : x);
}

static uint32_t mix(uint32_t x)
{
    x ^= x >> 16;
    x *= UINT32_C(0x7feb352d);
    x ^= x >> 15;
    x *= UINT32_C(0x846ca68b);
    return x ^ (x >> 16);
}

uint32_t terrain_hash(uint32_t seed, int x, int y, int z)
{
    return mix(seed ^ mix((uint32_t)x + UINT32_C(0xa511e9b3))
        ^ mix((uint32_t)y + UINT32_C(0x772df6fd)) ^ mix((uint32_t)z + UINT32_C(0x2d61cd31)));
}

uint32_t seed_hash(const char *text)
{
    const char *p = text;
    uint32_t n = 0;
    while (*p >= '0' && *p <= '9') n = n * 10u + (uint32_t)(*p++ - '0');
    if (p != text && !*p) return n;
    n = UINT32_C(2166136261);
    for (; *text; ++text) n = (n ^ (uint8_t)*text) * UINT32_C(16777619);
    return n;
}

const WorldSize default_world_size={4096,4096,48};

bool valid_world_size(WorldSize size)
{
    return size.width>=MIN_SIDE && size.width<=MAX_SIDE && size.width%(2*CHUNK)==0 &&
           size.depth>=MIN_SIDE && size.depth<=MAX_SIDE && size.depth%(2*CHUNK)==0 &&
           size.height>=MIN_HEIGHT && size.height<=MAX_HEIGHT;
}

int chunk_coordinate(int block)
{
    return block/CHUNK-(block%CHUNK<0);
}

unsigned cache_coordinate(int chunk)
{
    return (unsigned)chunk % CACHE_SIDE;
}

size_t chunk_offset(int x,int y,int z)
{
    return ((size_t)y*CHUNK+(size_t)z)*CHUNK+(size_t)x;
}

static bool in_columns(const World *w,int x,int z)
{
    return x>=w->min_x && x<w->max_x && z>=w->min_z && z<w->max_z;
}

bool inside(const World *w,int x,int y,int z)
{
    return (unsigned)y<(unsigned)w->size.height && in_columns(w,x,z);
}

static uint64_t next_revision(void)
{
    static uint64_t revision;
    return ++revision;
}

static void generate_chunk(const World *w,WorldChunk *chunk);

static size_t edit_position(const World *w,int cx,int cz)
{
    size_t lo=0,hi=w->edit_count;
    while (lo<hi) {
        const size_t mid=lo+(hi-lo)/2;
        const WorldChunk *c=w->edits[mid];
        if (c->z<cz || (c->z==cz && c->x<cx)) lo=mid+1;
        else hi=mid;
    }
    return lo;
}

static bool same_chunk(const WorldChunk *chunk,int cx,int cz)
{
    return chunk && chunk->x==cx && chunk->z==cz;
}

static WorldChunk *get_chunk(World *w,int cx,int cz)
{
    if (!in_columns(w,cx*CHUNK,cz*CHUNK)) return NULL;
    WorldChunk **slot=&w->cache[cache_coordinate(cz)][cache_coordinate(cx)];
    if (same_chunk(*slot,cx,cz)) return *slot;

    const size_t index=edit_position(w,cx,cz);
    WorldChunk *chunk=index<w->edit_count ? w->edits[index] : NULL;
    if (same_chunk(chunk,cx,cz)) {
        if (*slot && !(*slot)->edited) free(*slot);
        *slot=chunk;
        return chunk;
    }

    chunk=*slot && !(*slot)->edited ? *slot :
        malloc(sizeof(*chunk)+chunk_offset(0,w->size.height,0));
    if (!chunk) { w->failed=true; return NULL; }
    chunk->x=cx;
    chunk->z=cz;
    chunk->edited=false;
    chunk->revision=next_revision();
    generate_chunk(w,chunk);
    *slot=chunk;
    return chunk;
}

const WorldChunk *world_chunk(World *w,int cx,int cz)
{
    return get_chunk(w,cx,cz);
}

WorldChunk *edit_chunk(World *w,int cx,int cz)
{
    WorldChunk *chunk=get_chunk(w,cx,cz);
    if (!chunk || chunk->edited) return chunk;
    if (w->edit_count==w->edit_capacity) {
        const size_t capacity=w->edit_capacity ? w->edit_capacity*2 : 16;
        WorldChunk **edits=realloc(w->edits,capacity*sizeof(*edits));
        if (!edits) { w->failed=true; return NULL; }
        w->edits=edits;
        w->edit_capacity=capacity;
    }
    const size_t index=edit_position(w,cx,cz);
    memmove(w->edits+index+1,w->edits+index,(w->edit_count-index)*sizeof(*w->edits));
    w->edits[index]=chunk;
    ++w->edit_count;
    chunk->edited=true;
    chunk->revision=next_revision();
    return chunk;
}

void close_world(World *w)
{
    for (unsigned z=0; z<CACHE_SIDE; ++z) for (unsigned x=0; x<CACHE_SIDE; ++x) {
        WorldChunk *chunk=w->cache[z][x];
        if (chunk && !chunk->edited) free(chunk);
    }
    for (size_t n=0; n<w->edit_count; ++n) free(w->edits[n]);
    free(w->edits);
    memset(w,0,sizeof(*w));
}

int block_at(World *w,int x,int y,int z)
{
    if (!inside(w,x,y,z)) return AIR;
    const int cx=chunk_coordinate(x),cz=chunk_coordinate(z);
    const WorldChunk *chunk=get_chunk(w,cx,cz);
    return chunk ? chunk->cells[chunk_offset(x-cx*CHUNK,y,z-cz*CHUNK)] : AIR;
}

bool solid_at(World *w,int x,int y,int z)
{
    if (!in_columns(w,x,z) || y<0) return true;
    return blocks[block_at(w,x,y,z)].solid;
}

bool set_block(World *w,int x,int y,int z,int id)
{
    if (!inside(w,x,y,z) || y==0 || id<0 || id>=BLOCK_COUNT) return false;
    const int previous=block_at(w,x,y,z);
    if (previous==id) return false;
    const int cx=chunk_coordinate(x),cz=chunk_coordinate(z);
    WorldChunk *chunk=edit_chunk(w,cx,cz);
    if (!chunk) return false;
    chunk->cells[chunk_offset(x-cx*CHUNK,y,z-cz*CHUNK)]=(uint8_t)id;
    const int reach=(emits_light(id) || emits_light(previous)) ? LAMP_RADIUS+1 : 1;
    for (int j=chunk_coordinate(z-reach); j<=chunk_coordinate(z+reach); ++j) {
        for (int i=chunk_coordinate(x-reach); i<=chunk_coordinate(x+reach); ++i) {
            WorldChunk *neighbor=w->cache[cache_coordinate(j)][cache_coordinate(i)];
            if (same_chunk(neighbor,i,j)) neighbor->revision=next_revision();
        }
    }
    return true;
}

int surface_at(World *w,int x,int z)
{
    if (!in_columns(w,x,z)) return 0;
    const int cx=chunk_coordinate(x),cz=chunk_coordinate(z);
    const WorldChunk *chunk=get_chunk(w,cx,cz);
    if (chunk) {
        const size_t column=chunk_offset(x-cx*CHUNK,0,z-cz*CHUNK);
        for (int y=w->size.height-1; y>0; --y)
            if (chunk->cells[column+chunk_offset(0,y,0)]!=AIR) return y;
    }
    return 0;
}

static float random_grid(uint32_t seed, int x, int y, int z)
{
    return (float)(terrain_hash(seed,x,y,z) >> 8) / 16777216.0f;
}

static float smooth(float t) { return t*t*(3.0f-2.0f*t); }
static float lerp(float a, float b, float t) { return a + t*(b-a); }

static float layer(uint32_t seed, float x, int y, float z, int scale)
{
    x /= (float)scale;
    z /= (float)scale;
    const int ix = (int)floorf(x), iz = (int)floorf(z);
    const float tx = smooth(x-(float)ix), tz = smooth(z-(float)iz);
    const float a = lerp(random_grid(seed,ix,y,iz), random_grid(seed,ix+1,y,iz), tx);
    const float b = lerp(random_grid(seed,ix,y,iz+1), random_grid(seed,ix+1,y,iz+1), tx);
    return lerp(a,b,tz);
}

static float cave_noise(uint32_t seed, int x, int y, int z)
{
    const int band = y/7;
    return lerp(layer(seed,(float)x,band,(float)z,10),
                layer(seed,(float)x,band+1,(float)z,10), smooth((float)(y%7)/7.0f));
}

typedef struct {
    WorldChunk *chunk;
    int height,x0,z0;
} Generator;

static uint8_t *generated_cell(Generator *g,int x,int y,int z)
{
    const unsigned lx=(unsigned)(x-g->x0),lz=(unsigned)(z-g->z0);
    if (lx>=CHUNK || lz>=CHUNK || (unsigned)y>=(unsigned)g->height) return NULL;
    return g->chunk->cells+chunk_offset((int)lx,y,(int)lz);
}

static void generate_block(Generator *g,int x,int y,int z,int id)
{
    uint8_t *cell=generated_cell(g,x,y,z);
    if (cell) *cell=(uint8_t)id;
}

static int generated_block_at(Generator *g,int x,int y,int z)
{
    const uint8_t *cell=generated_cell(g,x,y,z);
    return cell ? *cell : AIR;
}

// Pino de las zonas frias: tronco alto y copa de agujas en capas que alternan de radio.
static void pine(Generator *w, int x, int y, int z, uint32_t hash)
{
    const int tall = 5 + (int)(hash & 3u);
    for (int dy=1; dy<=tall+1; ++dy) generate_block(w,x,y+dy,z,PINE_LOG);
    for (int dy=2; dy<=tall+2; ++dy) {
        const int r = dy>=tall+1 ? 0 : (((tall-dy)&1) ? 1 : 2);
        for (int dz=-r; dz<=r; ++dz) for (int dx=-r; dx<=r; ++dx) {
            if (r==2 && abs(dx)+abs(dz)==4) continue;
            if (generated_block_at(w,x+dx,y+dy,z+dz)==AIR) generate_block(w,x+dx,y+dy,z+dz,NEEDLES);
        }
    }
}

static void tree(Generator *w, int x, int y, int z, uint32_t hash)
{
    const bool birch = (hash & 4u) != 0;
    const int trunk = birch ? BIRCH : LOG;
    const int leaf = birch ? AUTUMN : LEAVES;
    const int tall = 3 + (int)(hash & 1u);
    for (int dy=1; dy<=tall; ++dy) generate_block(w,x,y+dy,z,trunk);
    for (int dy=tall-1; dy<=tall+2; ++dy) {
        const int r = dy>tall ? 1 : 2;
        for (int dz=-r; dz<=r; ++dz) for (int dx=-r; dx<=r; ++dx) {
            if (abs(dx)+abs(dz)==2*r && (dy==tall+2 || (hash>>((dx+2)*3+dz+2))&1u)) continue;
            if (generated_block_at(w,x+dx,y+dy,z+dz)==AIR) generate_block(w,x+dx,y+dy,z+dz,leaf);
        }
    }
}

static void lodge(Generator *w, int x, int y, int z, bool red)
{
    int siding = red ? RED_PLANK : PLANK;
    for (int dz=0; dz<7; ++dz) for (int dx=0; dx<7; ++dx) {
        generate_block(w,x+dx,y,z+dz,COBBLE);
        const bool corner = (dx==0 || dx==6) && (dz==0 || dz==6);
        const bool wall = dx==0 || dx==6 || dz==0 || dz==6;
        for (int dy=1; dy<=3; ++dy) {
            int id = corner ? LOG : (wall ? siding : AIR);
            if (wall && dy==2 && (dx==2 || dx==4 || dz==2 || dz==4)) id = GLASS;
            if (dx==3 && dz==6 && dy<3) id = AIR;
            generate_block(w,x+dx,y+dy,z+dz,id);
        }
    }
    for (int row=0; row<4; ++row) {
        for (int dz=-1; dz<8; ++dz) {
            generate_block(w,x+row,y+4+row,z+dz,BRICK);
            generate_block(w,x+6-row,y+4+row,z+dz,BRICK);
        }
        for (int dx=row+1; dx<6-row; ++dx) {
            generate_block(w,x+dx,y+4+row,z,siding);
            generate_block(w,x+dx,y+4+row,z+6,siding);
        }
    }
    generate_block(w,x+2,y+1,z+2,WORKBENCH);
    generate_block(w,x+4,y+1,z+2,LANTERN);
    generate_block(w,x+5,y+1,z+2,FURNACE);
}

static void mine_entrance(Generator *w)
{
    for (int x=69; x<=77; ++x) for (int z=87; z<=88; ++z)
        generate_block(w,x,16,z,GRAVEL);
    for (int z=86; z<=89; ++z) {
        if (z==86 || z==89) for (int y=17; y<=19; ++y) generate_block(w,68,y,z,LOG);
        generate_block(w,68,20,z,LOG);
    }
    generate_block(w,68,17,85,LANTERN);
    generate_block(w,68,17,90,LANTERN);
    for (int step=0; step<=14; ++step) {
        const int x=68-step, y=16-(step<9?step:9);
        for (int z=87; z<=88; ++z) {
            generate_block(w,x,y,z,step%2 ? COBBLE : STONE);
            for (int dy=1; dy<=4; ++dy) generate_block(w,x,y+dy,z,AIR);
        }
    }
    generate_block(w,55,8,86,LANTERN);
    generate_block(w,53,8,87,IRON);
    generate_block(w,53,9,87,COAL);
    generate_block(w,54,8,89,GOLD);
}

static void build_village(Generator *w)
{
    for (int z=70; z<=94; ++z) for (int x=77; x<=81; ++x) {
        if (x>=78 && x<=80) generate_block(w,x,16,z,GRAVEL);
        else if (z==73 || z==80 || z==88) generate_block(w,x,17,z,LANTERN);
    }
    for (int x=72; x<=89; ++x) for (int z=76; z<=78; ++z) generate_block(w,x,16,z,GRAVEL);
    lodge(w,70,16,69,false);
    lodge(w,83,16,69,true);
    lodge(w,83,16,81,false);
    mine_entrance(w);
}

typedef struct { int top,cap; float temperature,village; } Column;

static Column terrain_column(const World *w,int x,int z)
{
    const uint32_t seed=w->seed;
    const float large=layer(seed,(float)x,12,(float)z,45);
    const float small=layer(seed,(float)x,28,(float)z,15);
    const float detail=layer(seed,(float)x,44,(float)z,5);
    const float temp=layer(seed^128u,(float)x,9,(float)z,47);
    const float dx=(float)(x-80),dz=(float)(z-79);
    const float distance=sqrtf(dx*dx+dz*dz);
    const float village=clampf((26.0f-distance)/10.0f,0,1);
    float elevation=4.0f+23.0f*large+6.0f*small+2.0f*detail;
    const float region=smooth(clampf((distance-120.0f)/130.0f,0,1));
    if (region>0) {
        const float continent=layer(seed^173u,(float)x,51,(float)z,210);
        const float peaks=clampf((continent-.55f)*2.3f,0,1);
        elevation+=region*((continent-.49f)*15.0f +
            peaks*large*(float)(w->size.height-MIN_HEIGHT));
    }
    const int top=(int)clampf(lerp(elevation,16.0f,village),3,(float)(w->size.height-7));
    int cap=GRASS;
    if (top<=WATERLINE+1) cap=temp>.66f ? RED_SAND : SAND;
    else if (top>=24) cap=top>=27 ? SNOW : STONE;
    else if (temp>.72f && village<.5f) cap=RED_SAND;
    return (Column){top,cap,temp,village};
}

static void generate_chunk(const World *w,WorldChunk *chunk)
{
    const int x0=chunk->x*CHUNK,z0=chunk->z*CHUNK;
    const uint32_t seed=w->seed;
    Generator gen={chunk,w->size.height,x0,z0};
    memset(chunk->cells,0,chunk_offset(0,w->size.height,0));
    for (int z=z0; z<z0+CHUNK; ++z) for (int x=x0; x<x0+CHUNK; ++x) {
        const Column column=terrain_column(w,x,z);
        const int top=column.top,cap=column.cap;
        generate_block(&gen,x,0,z,BEDROCK);
        for (int y=1; y<=top; ++y) {
            int id=STONE;
            uint32_t roll=terrain_hash(seed,x,y,z)%1000u;
            const uint32_t extra=terrain_hash(seed^UINT32_C(0x5bd1e995),x,y,z)%1000u;
            if (y==top) id=cap;
            else if (y>=top-3) id=(cap==SAND || cap==RED_SAND) ? (y<top-1 ? SANDSTONE : cap) : DIRT;
            else if (y>=3 && column.village<.95f && cave_noise(seed^541u,x,y,z)>.74f) id=y<=5 ? LAVA : AIR;
            else if (roll<15u) id=COAL;
            else if (roll<24u && y<16) id=IRON;
            else if (roll<28u && y<12) id=GOLD;
            else if (roll<31u && y<9) id=AMBER;
            else if (roll<45u) id=GRAVEL;
            else if (roll<57u) id=CLAY;
            else if (roll<64u) id=MOSS;
            // Minerales nuevos con una segunda tirada: la distribucion de los antiguos no cambia.
            if (id==STONE) {
                if (extra<13u && y>=4 && y<22) id=COPPER_ORE;
                else if (extra>=994u && y<8) id=DIAMOND_ORE;
                else if (extra>=960u && extra<976u && y<=6) id=OBSIDIAN;
            }
            generate_block(&gen,x,y,z,id);
        }
        for (int y=top+1; y<=WATERLINE; ++y)
            generate_block(&gen,x,y,z,column.temperature<.25f && y==WATERLINE ? ICE : WATER);
    }
    // Include tree roots two blocks across a border, in the same order on both sides.
    for (int z=z0-2; z<z0+CHUNK+2; ++z) for (int x=x0-2; x<x0+CHUNK+2; ++x) {
        if (!in_columns(w,x,z) || (x>64 && x<99 && z>62 && z<100)) continue;
        const uint32_t r=terrain_hash(seed^923u,x,0,z);
        if (r%23u!=0) continue;
        const Column column=terrain_column(w,x,z);
        if (column.cap==GRASS) {
            if (column.temperature<.33f) pine(&gen,x,column.top,z,r);
            else tree(&gen,x,column.top,z,r);
        }
    }
    if (x0>=64-CHUNK && x0<96 && z0>=64 && z0<96) build_village(&gen);
}

void generate(World *w,uint32_t seed,WorldSize size)
{
    w->size=size;
    w->seed=seed;
    w->min_x=80-size.width/2;
    w->min_z=80-size.depth/2;
    w->max_x=w->min_x+size.width;
    w->max_z=w->min_z+size.depth;
    w->start=(Vec3){79.5f,17.01f,91.5f};
}

Hit raycast(World *w, Vec3 p, Vec3 d, float reach)
{
    const float values[3]={p.x,p.y,p.z}, dirs[3]={d.x,d.y,d.z};
    int cell[3]={(int)floorf(p.x),(int)floorf(p.y),(int)floorf(p.z)}, step[3];
    float delta[3], next[3];
    Hit result = {0};
    for (int i=0; i<3; ++i) {
        step[i] = dirs[i]<0 ? -1 : 1;
        delta[i] = dirs[i]==0 ? INFINITY : fabsf(1.0f/dirs[i]);
        next[i] = dirs[i]==0 ? INFINITY :
            ((float)(cell[i]+(step[i]>0))-values[i])/dirs[i];
    }
    float at = 0;
    while (at<=reach) {
        const int block = block_at(w,cell[0],cell[1],cell[2]);
        if (block!=AIR && block!=WATER && block!=LAVA) {
            result.x=cell[0]; result.y=cell[1]; result.z=cell[2];
            result.found=true; result.distance=at;
            return result;
        }
        result.px=cell[0]; result.py=cell[1]; result.pz=cell[2];
        const int first = next[0]<next[1] ? 0 : 1;
        const int axis = next[first]<next[2] ? first : 2;
        at = next[axis];
        next[axis] += delta[axis];
        cell[axis] += step[axis];
        result.adjacent=true;
    }
    result.adjacent=false;
    return result;
}
