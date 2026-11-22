#ifndef BRUMA_GAME_H
#define BRUMA_GAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CHUNK 16
#define LEGACY_SIDE 160
#define MIN_SIDE LEGACY_SIDE
#define MAX_SIDE 32768
#define MIN_HEIGHT 48
#define MAX_HEIGHT 128
#define CACHE_SIDE 16
#define WATERLINE 13
#define LAMP_RADIUS 5
#define PI 3.14159265358979323846f
#define MAX_MOBS 64
#define MAX_BOLTS 24
#define SLOTS 9

typedef enum {
    AIR, GRASS, DIRT, STONE, SAND, RED_SAND, GRAVEL, CLAY, LOG, BIRCH,
    LEAVES, AUTUMN, PLANK, RED_PLANK, BRICK, COBBLE, MOSS, GLASS, WATER,
    SNOW, ICE, COAL, IRON, GOLD, AMBER, CLOTH, WORKBENCH, LANTERN, BEDROCK,
    /* Bloques nuevos (D449): van detras de BEDROCK para que las partidas antiguas sigan cargando. */
    COPPER_ORE, DIAMOND_ORE, OBSIDIAN, PINE_LOG, PINE_PLANK, NEEDLES, SANDSTONE,
    STONE_BRICK, FURNACE, LAVA,
    BLOCK_COUNT,
    PICK = BLOCK_COUNT, AXE, SHOVEL, BLADE, BERRIES,
    INGOT_COPPER, INGOT_IRON, GEM,
    PICK_IRON, AXE_IRON, BLADE_IRON, PICK_GEM, AXE_GEM, BLADE_GEM,
    ITEM_COUNT
} Item;
/* Numeracion de objetos de las partidas versiones 1 y 2 (antes de D449): 0-28 bloques, 29-33 herramientas y fruta. */
#define OLD_BLOCK_COUNT 29
#define OLD_ITEM_COUNT 34

enum { F_EAST, F_WEST, F_UP, F_DOWN, F_SOUTH, F_NORTH };

typedef struct { float x, y, z; } Vec3;
typedef struct {
    const char *name;
    int top, side, bottom;
    float hardness;
    uint8_t opacity;
    bool solid;
} Block;
extern const Block blocks[BLOCK_COUNT];
extern const int item_icons[ITEM_COUNT];
static inline bool emits_light(int id) { return id==LANTERN || id==LAVA; }
/* Herramientas: tipo (PICK, AXE, SHOVEL, BLADE; 0 si no lo es), nivel (1 base, 2 hierro, 3 diamante) y vida util. */
int tool_kind(int item);
int tool_tier(int item);
int tool_life(int item);
int old_item_id(int old);
const char *item_name(int item);
float clampf(float x, float min, float max);
uint32_t seed_hash(const char *text);
uint32_t terrain_hash(uint32_t seed, int x, int y, int z);

typedef struct { int width, depth, height; } WorldSize;
extern const WorldSize default_world_size;
bool valid_world_size(WorldSize size);
int chunk_coordinate(int block);
unsigned cache_coordinate(int chunk);
size_t chunk_offset(int x,int y,int z);

typedef struct {
    int x,z;
    uint64_t revision;
    bool edited;
    uint8_t cells[];
} WorldChunk;

typedef struct {
    WorldSize size;
    int min_x,min_z,max_x,max_z;
    WorldChunk *cache[CACHE_SIDE][CACHE_SIDE];
    WorldChunk **edits;
    size_t edit_count,edit_capacity;
    uint32_t seed;
    Vec3 start;
    bool failed;
} World;

bool inside(const World *world, int x, int y, int z);
const WorldChunk *world_chunk(World *world, int cx, int cz);
WorldChunk *edit_chunk(World *world, int cx, int cz);
int block_at(World *world, int x, int y, int z);
bool set_block(World *world, int x, int y, int z, int id);
bool solid_at(World *world, int x, int y, int z);
int surface_at(World *world, int x, int z);
void generate(World *world, uint32_t seed, WorldSize size);
void close_world(World *world);

typedef struct { int x,y,z, px,py,pz; float distance; bool found, adjacent; } Hit;
Hit raycast(World *world, Vec3 origin, Vec3 direction, float reach);

typedef enum { FOX, BOAR, HARE, VILLAGER, SHADE, GOLEM, WISP, MOB_COUNT } Species;
typedef struct {
    Vec3 pos;
    float yaw, vy, phase, wander, cooldown, hurt, flee;
    int hp;
    Species kind;
    bool active;
} Mob;
typedef struct { Vec3 pos, velocity; float lifetime; } Bolt;

typedef struct {
    World world;
    Vec3 pos;
    float yaw, pitch, vy, time, hurt, step_clock;
    int health;
    bool creative, grounded, swimming;
    uint16_t bag[ITEM_COUNT];
    uint8_t slots[SLOTS];
    int selected;
    Mob mobs[MAX_MOBS];
    Bolt bolts[MAX_BOLTS];
    uint32_t random;
    int wildlife_x,wildlife_z;
    float respawn_clock, action_delay, mining;
    Hit aimed, mined;
    int mob_target;
    char notice[128];
    float notice_time;
    int sound;
    uint16_t wear[ITEM_COUNT];   /* usos gastados de la herramienta en uso, por tipo de objeto */
} Game;

enum { SND_NONE, SND_STEP, SND_BREAK, SND_PLACE, SND_HIT, SND_HURT, SND_CRAFT };
typedef struct {
    float forward, side;
    bool jump, sprint, mine, attack, place, interact;
} Input;
typedef struct {
    int output, quantity;
    int first, count_first;
    int second, count_second;
    int need;   /* objeto que hay que llevar en la mochila (sin gastarlo), p. ej. el horno; AIR si ninguno */
} Recipe;
extern const Recipe recipes[];
extern const size_t recipe_count;
extern const char *const mob_names[MOB_COUNT];

Vec3 view_direction(const Game *game);
float daylight(const Game *game);
bool body_blocked(World *world, Vec3 pos, float radius, float height);
void new_game(Game *game, uint32_t seed, WorldSize size);
void refresh_wildlife(Game *game, bool force);
void tick(Game *game, Input input, float dt);
void aim(Game *game);
void notify(Game *game, const char *message);
void give(Game *game, int item, unsigned quantity);
bool craft(Game *game, size_t recipe);
bool save_game(const Game *game, const char *path);
bool load_game(Game *game, const char *path, uint32_t seed, WorldSize size);
bool save_exists(const char *path);

#endif
