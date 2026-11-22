#include "game.h"

#undef NDEBUG
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_generation(Game *a, Game *b)
{
    const uint32_t seed=seed_hash("Bruma-160");
    assert(seed_hash("123") == 123);
    const WorldSize size={32768,16384,96};
    assert(valid_world_size(size));
    assert(!valid_world_size((WorldSize){1025,1024,48}));
    new_game(a,seed,size);
    new_game(b,seed,size);
    const int cx=chunk_coordinate(-10017),cz=chunk_coordinate(6950);
    const size_t bytes=chunk_offset(0,size.height,0);
    const WorldChunk *chunk=world_chunk(&a->world,cx,cz);
    assert(chunk && world_chunk(&b->world,cx,cz));
    assert(memcmp(chunk->cells,world_chunk(&b->world,cx,cz)->cells,bytes)==0);
    new_game(b,seed+1,size);
    assert(memcmp(chunk->cells,world_chunk(&b->world,cx,cz)->cells,bytes)!=0);
    assert(!body_blocked(&a->world,a->pos,.29f,1.75f));
    assert(!inside(&a->world,a->world.min_x-1,0,0));
    assert(solid_at(&a->world,a->world.min_x-1,40,0));
    assert(world_chunk(&a->world,chunk_coordinate(a->world.max_x),cz)==NULL);

    bool seen[MOB_COUNT]={0};
    for (int i=0; i<MAX_MOBS; ++i)
        if (a->mobs[i].active) seen[a->mobs[i].kind]=true;
    for (int i=0; i<MOB_COUNT; ++i) assert(seen[i]);

    uint64_t before[2][2];
    for (int z=1; z<=2; ++z) for (int x=1; x<=2; ++x)
        before[z-1][x-1]=world_chunk(&a->world,x,z)->revision;
    assert(set_block(&a->world,32,44,32,PLANK));
    for (int z=1; z<=2; ++z) for (int x=1; x<=2; ++x)
        assert(world_chunk(&a->world,x,z)->revision>before[z-1][x-1]);
    assert(!set_block(&a->world,32,0,32,AIR));
    assert(!set_block(&a->world,a->world.min_x-1,44,32,STONE));
    int far_x=cx*CHUNK+15,far_z=cz*CHUNK+8;
    assert(set_block(&a->world,far_x,75,far_z,LANTERN));
    assert(world_chunk(&a->world,cx+CACHE_SIDE,cz));
    assert(block_at(&a->world,far_x,75,far_z)==LANTERN);

    const Hit hit=raycast(&a->world,(Vec3){29.5f,44.5f,32.5f},(Vec3){1,0,0},5);
    assert(hit.found && hit.adjacent && hit.x==32 && hit.px==31);
    assert(fabsf(hit.distance-2.5f)<.001f);
}

static void test_gameplay(Game *a)
{
    memset(a->mobs,0,sizeof(a->mobs));
    a->pos=(Vec3){78.5f,17.01f,91.5f};
    a->yaw=0;
    a->pitch=0;
    a->selected=3;
    assert(a->slots[a->selected]==PLANK);
    assert(set_block(&a->world,78,18,88,STONE));
    const uint16_t before=a->bag[PLANK];
    tick(a,(Input){.place=true},1.0f/120);
    assert(block_at(&a->world,78,18,89)==PLANK);
    assert(a->bag[PLANK]==before-1);
    a->selected=1;
    assert(a->slots[a->selected]==AXE);
    for (int i=0; i<30; ++i) tick(a,(Input){.mine=true},1.0f/120);
    assert(block_at(&a->world,78,18,89)==AIR);
    assert(a->bag[PLANK]==before);

    give(a,IRON,2);
    assert(craft(a,5));
    assert(a->bag[BLADE]==1 && a->bag[IRON]==0);
    assert(!craft(a,5));
    Vec3 p=a->pos;
    for (int i=0; i<36; ++i) tick(a,(Input){.jump=true},1.0f/120);
    assert(a->pos.y>p.y+1);
}

static void test_save(Game *a,Game *b,const char *path)
{
    const WorldSize size=a->world.size;
    a->pos=(Vec3){-10016.5f,91.5f,6950.5f};
    assert(save_game(a,path));
    assert(load_game(b,path,a->world.seed,size));
    assert(b->health==a->health && b->selected==a->selected && b->bag[BLADE]==1);
    assert(a->pos.x==b->pos.x && a->pos.y==b->pos.y && a->yaw==b->yaw);
    assert(a->world.edit_count==b->world.edit_count);
    const size_t bytes=chunk_offset(0,size.height,0);
    for (size_t i=0; i<a->world.edit_count; ++i)
        assert(memcmp(a->world.edits[i]->cells,b->world.edits[i]->cells,bytes)==0);
    assert(!load_game(b,path,a->world.seed+1,size));
    assert(!load_game(b,path,a->world.seed,default_world_size));
    FILE *broken=fopen(path,"wb");
    assert(broken);
    assert(fputs("BRUMA",broken)>=0 && fclose(broken)==0);
    assert(!load_game(b,path,a->world.seed,size));
    assert(b->pos.y==a->pos.y && b->bag[BLADE]==1);
    assert(remove(path)==0);
}


// ---- D449: bloques, recetas, herramientas con desgaste y partidas antiguas

static void count_blocks(World *w,int x0,int z0,int side,int *count)
{
    memset(count,0,sizeof(int)*BLOCK_COUNT);
    for (int z=z0; z<z0+side; ++z) for (int x=x0; x<x0+side; ++x)
        for (int y=0; y<w->size.height; ++y) ++count[block_at(w,x,y,z)];
}

static void test_content(Game *a)
{
    int count[BLOCK_COUNT];
    // Los minerales y bloques nuevos aparecen al generar (zona amplia, semilla fija).
    int total[BLOCK_COUNT]={0};
    for (int z=-160; z<400; z+=64) for (int x=-160; x<400; x+=64) {
        count_blocks(&a->world,x,z,64,count);
        for (int i=0; i<BLOCK_COUNT; ++i) total[i]+=count[i];
    }
    assert(total[COPPER_ORE]>0 && total[DIAMOND_ORE]>0 && total[OBSIDIAN]>0);
    assert(total[LAVA]>0 && total[SANDSTONE]>0 && total[FURNACE]>0);
    assert(total[PINE_LOG]>0 && total[NEEDLES]>0);
    assert(total[COAL]>0 && total[IRON]>0);   // los antiguos siguen
    printf("   bloques: cobre %d, diamante %d, obsidiana %d, lava %d, arenisca %d, pino %d\n",
        total[COPPER_ORE],total[DIAMOND_ORE],total[OBSIDIAN],total[LAVA],total[SANDSTONE],total[PINE_LOG]);
    // Cada bloque tiene nombre e icono, y el horno y la lava se comportan como se espera.
    for (int i=1; i<ITEM_COUNT; ++i) assert(item_icons[i]>=0 && item_icons[i]<63 && item_name(i)[0]);
    assert(blocks[LAVA].solid==false && blocks[FURNACE].solid && emits_light(LAVA));
}

static void test_tools(Game *a)
{
    memset(a->mobs,0,sizeof(a->mobs));
    a->creative=false;
    memset(a->bag,0,sizeof(a->bag));
    memset(a->wear,0,sizeof(a->wear));
    memset(a->slots,0,sizeof(a->slots));
    a->pos=(Vec3){78.5f,17.01f,91.5f}; a->yaw=0; a->pitch=0; a->health=20;
    // Fundir sin horno falla; con el horno, el hierro y el carbon dan un lingote.
    give(a,IRON,1); give(a,COAL,1);
    size_t smelt=0;
    for (size_t i=0; i<recipe_count; ++i) if (recipes[i].output==INGOT_IRON) smelt=i;
    assert(smelt && recipes[smelt].need==FURNACE);
    assert(!craft(a,smelt));
    give(a,FURNACE,1);
    assert(craft(a,smelt) && a->bag[INGOT_IRON]==1 && a->bag[IRON]==0 && a->bag[COAL]==0 && a->bag[FURNACE]==1);
    // Herramienta de hierro: tres lingotes y dos tablas.
    give(a,INGOT_IRON,2); give(a,PLANK,2);
    size_t pick=0;
    for (size_t i=0; i<recipe_count; ++i) if (recipes[i].output==PICK_IRON) pick=i;
    assert(pick && craft(a,pick) && a->bag[PICK_IRON]==1);
    assert(tool_tier(PICK_IRON)==2 && tool_kind(PICK_GEM)==PICK && tool_life(PICK_GEM)>tool_life(PICK_IRON));

    // El diamante exige pico de hierro: con la mano no sale; con el pico de hierro da una gema y gasta un uso.
    assert(set_block(&a->world,78,18,89,DIAMOND_ORE));
    a->slots[0]=AIR; a->selected=0;
    for (int i=0; i<400; ++i) tick(a,(Input){.mine=true},1.0f/60);
    assert(block_at(&a->world,78,18,89)==DIAMOND_ORE && a->bag[GEM]==0);
    give(a,PICK,1); a->slots[1]=PICK; a->selected=1;
    for (int i=0; i<400; ++i) tick(a,(Input){.mine=true},1.0f/60);
    assert(block_at(&a->world,78,18,89)==DIAMOND_ORE);
    a->slots[2]=PICK_IRON; a->selected=2;
    for (int i=0; i<400 && block_at(&a->world,78,18,89)==DIAMOND_ORE; ++i) tick(a,(Input){.mine=true},1.0f/60);
    assert(block_at(&a->world,78,18,89)==AIR && a->bag[GEM]==1 && a->wear[PICK_IRON]==1);

    // La herramienta se rompe al agotar su vida util y se pierde una unidad.
    a->bag[PICK]=1; a->wear[PICK]=(uint16_t)(tool_life(PICK)-1); a->selected=1;
    assert(set_block(&a->world,78,18,89,STONE));
    for (int i=0; i<400 && a->bag[PICK]; ++i) tick(a,(Input){.mine=true},1.0f/60);
    assert(a->bag[PICK]==0 && a->wear[PICK]==0);

    // La lava quema.
    a->health=20; a->hurt=0;
    assert(set_block(&a->world,78,17,91,LAVA) && set_block(&a->world,78,18,91,LAVA));
    a->pos=(Vec3){78.5f,17.2f,91.5f};
    for (int i=0; i<30 && a->health==20; ++i) tick(a,(Input){0},1.0f/60);
    assert(a->health<20);
}

static void put_int(FILE *f,uint32_t n) { fwrite(&n,4,1,f); }
static void put_float(FILE *f,float v) { fwrite(&v,4,1,f); }

// Una partida de la version 2 (antes de D449) debe cargar con los objetos renumerados.
static void test_old_save(Game *g,const char *path)
{
    const WorldSize size={160,160,48};
    const uint32_t seed=seed_hash("Antigua");
    FILE *f=fopen(path,"wb");
    assert(f);
    const unsigned char magic[8]={'B','R','U','M','A',0,2,0};
    fwrite(magic,1,8,f);
    put_int(f,seed); put_int(f,160); put_int(f,160); put_int(f,48);
    put_int(f,17); put_int(f,2); put_int(f,0);
    put_float(f,79.5f); put_float(f,30.0f); put_float(f,91.5f); put_float(f,0); put_float(f,0); put_float(f,.25f);
    for (int i=0; i<OLD_ITEM_COUNT; ++i) put_int(f,i==29?1:(i==33?7:(i==PLANK?9:0)));   // 29 era el pico, 33 la fruta
    const unsigned char slots[SLOTS]={29,PLANK,33,0,0,0,0,0,0};
    fwrite(slots,1,SLOTS,f);
    put_int(f,0);
    fclose(f);
    assert(load_game(g,path,seed,size));
    assert(g->health==17 && g->bag[PICK]==1 && g->bag[BERRIES]==7 && g->bag[PLANK]==9);
    assert(g->slots[0]==PICK && g->slots[1]==PLANK && g->slots[2]==BERRIES);
    assert(g->wear[PICK]==0);
    assert(remove(path)==0);
}

int main(int argc,char **argv)
{
    assert(argc==2);
    Game *a=calloc(1,sizeof(*a)), *b=calloc(1,sizeof(*b));
    assert(a && b);
    test_generation(a,b);
    test_gameplay(a);
    test_save(a,b,argv[1]);
    new_game(a,seed_hash("Bruma-160"),(WorldSize){160,160,48});
    test_content(a);
    test_tools(a);
    test_old_save(b,argv[1]);
    close_world(&a->world);
    close_world(&b->world);
    free(a);
    free(b);
    puts("OK: tamanos, semillas, sectores, especies, mineria, construccion, fabricacion, guardado, minerales, horno, herramientas con desgaste, lava y partidas antiguas.");
    return 0;
}
