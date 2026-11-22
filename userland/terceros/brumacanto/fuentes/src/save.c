#include "game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>

static wchar_t *wide_path(const char *path)
{
    const int length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,NULL,0);
    if (length<=0) return NULL;
    wchar_t *wide=malloc((size_t)length*sizeof(*wide));
    if (!wide) return NULL;
    if (!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,length)) {
        free(wide);
        return NULL;
    }
    return wide;
}

static FILE *open_save(const char *path,bool writing)
{
    wchar_t *wide=wide_path(path);
    FILE *file=wide ? _wfopen(wide,writing?L"wb":L"rb") : NULL;
    free(wide);
    return file;
}

static bool replace_save(const char *temp,const char *path,bool ok)
{
    wchar_t *wide_temp=wide_path(temp);
    wchar_t *wide_pathname=wide_path(path);
    ok=ok && wide_temp && wide_pathname &&
        MoveFileExW(wide_temp,wide_pathname,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
    if (!ok && wide_temp) _wremove(wide_temp);
    free(wide_temp);
    free(wide_pathname);
    return ok;
}
#else
#include <unistd.h>

static FILE *open_save(const char *path,bool writing)
{
    return fopen(path,writing?"wb":"rb");
}

static bool replace_save(const char *temp,const char *path,bool ok)
{
    ok=ok && rename(temp,path)==0;
    if (!ok) remove(temp);
    return ok;
}
#endif

static const unsigned char magic[8]={'B','R','U','M','A',0,3,0};

static bool write_int(FILE *file, uint32_t n)
{
    unsigned char bytes[4]={(unsigned char)n,(unsigned char)(n>>8),
        (unsigned char)(n>>16),(unsigned char)(n>>24)};
    return fwrite(bytes,1,4,file)==4;
}

static bool read_int(FILE *file, uint32_t *n)
{
    unsigned char bytes[4];
    if (fread(bytes,1,4,file)!=4) return false;
    *n=(uint32_t)bytes[0]|((uint32_t)bytes[1]<<8)|
        ((uint32_t)bytes[2]<<16)|((uint32_t)bytes[3]<<24);
    return true;
}

static bool write_float(FILE *file, float v)
{
    uint32_t n;
    memcpy(&n,&v,sizeof(n));
    return write_int(file,n);
}

static bool read_float(FILE *file, float *v)
{
    uint32_t n;
    if (!read_int(file,&n)) return false;
    memcpy(v,&n,sizeof(n));
    return isfinite(*v);
}

bool save_exists(const char *path)
{
    FILE *file=open_save(path,false);
    if (!file) return false;
    fclose(file);
    return true;
}

bool save_game(const Game *g, const char *path)
{
    const size_t len=strlen(path)+32;
    char *temp=malloc(len);
    if (!temp) return false;
#ifdef _WIN32
    const unsigned long pid=(unsigned long)GetCurrentProcessId();
#else
    const unsigned long pid=(unsigned long)getpid();
#endif
    snprintf(temp,len,"%s.%lu.tmp",path,pid);
    FILE *file=open_save(temp,true);
    bool ok=false;
    if (file) {
        ok=fwrite(magic,1,sizeof(magic),file)==sizeof(magic)
            && write_int(file,g->world.seed)
            && write_int(file,(uint32_t)g->world.size.width)
            && write_int(file,(uint32_t)g->world.size.depth)
            && write_int(file,(uint32_t)g->world.size.height)
            && write_int(file,(uint32_t)g->health)
            && write_int(file,(uint32_t)g->selected) && write_int(file,(uint32_t)g->creative)
            && write_float(file,g->pos.x) && write_float(file,g->pos.y)
            && write_float(file,g->pos.z) && write_float(file,g->yaw)
            && write_float(file,g->pitch) && write_float(file,g->time);
        for (int i=0; ok && i<ITEM_COUNT; ++i) ok=write_int(file,g->bag[i]);
        ok=ok && fwrite(g->slots,1,SLOTS,file)==SLOTS;
        for (int i=0; ok && i<ITEM_COUNT; ++i) ok=write_int(file,g->wear[i]);   // desgaste de herramientas (version 3)
        ok=ok && write_int(file,(uint32_t)g->world.edit_count);
        const size_t bytes=chunk_offset(0,g->world.size.height,0);
        const int min_x=chunk_coordinate(g->world.min_x),min_z=chunk_coordinate(g->world.min_z);
        for (size_t i=0; ok && i<g->world.edit_count; ++i) {
            const WorldChunk *chunk=g->world.edits[i];
            ok=write_int(file,(uint32_t)(chunk->x-min_x)) && write_int(file,(uint32_t)(chunk->z-min_z))
                && fwrite(chunk->cells,1,bytes,file)==bytes;
        }
        if (fclose(file)!=0) ok=false;
        ok=replace_save(temp,path,ok);
    }
    free(temp);
    return ok;
}

static bool read_legacy_chunks(FILE *file,World *w)
{
    enum { LEGACY_CHUNKS=LEGACY_SIDE/CHUNK };
    WorldChunk *chunks[LEGACY_CHUNKS][LEGACY_CHUNKS];
    for (int z=0; z<LEGACY_CHUNKS; ++z) for (int x=0; x<LEGACY_CHUNKS; ++x) {
        chunks[z][x]=edit_chunk(w,x,z);
        if (!chunks[z][x]) return false;
    }
    unsigned char row[LEGACY_SIDE];
    for (int y=0; y<MIN_HEIGHT; ++y) for (int z=0; z<LEGACY_SIDE; ++z) {
        if (fread(row,1,sizeof(row),file)!=sizeof(row)) return false;
        for (int x=0; x<LEGACY_SIDE; ++x)
            if (row[x]>=BLOCK_COUNT || (y==0 && row[x]!=BEDROCK)) return false;
        for (int x=0; x<LEGACY_CHUNKS; ++x)
            memcpy(chunks[z/CHUNK][x]->cells+chunk_offset(0,y,z%CHUNK),row+x*CHUNK,CHUNK);
    }
    return true;
}

static bool read_chunks(FILE *file,World *w)
{
    uint32_t count;
    const uint32_t cols=(uint32_t)(w->size.width/CHUNK),rows=(uint32_t)(w->size.depth/CHUNK);
    if (!read_int(file,&count) || count>cols*rows) return false;
    const int min_x=chunk_coordinate(w->min_x),min_z=chunk_coordinate(w->min_z);
    const size_t bytes=chunk_offset(0,w->size.height,0);
    uint32_t previous=0;
    for (uint32_t i=0; i<count; ++i) {
        uint32_t x,z;
        if (!read_int(file,&x) || x>=cols || !read_int(file,&z) || z>=rows) return false;
        const uint32_t order=z*cols+x;
        if (i>0 && order<=previous) return false;
        previous=order;
        WorldChunk *chunk=edit_chunk(w,min_x+(int)x,min_z+(int)z);
        if (!chunk || fread(chunk->cells,1,bytes,file)!=bytes) return false;
        for (size_t j=0; j<bytes; ++j) {
            const int id=chunk->cells[j];
            if (id>=BLOCK_COUNT || (j<CHUNK*CHUNK && id!=BEDROCK)) return false;
        }
    }
    return true;
}

static bool read_size(FILE *file,WorldSize size)
{
    uint32_t width,depth,height;
    return read_int(file,&width) && width==(uint32_t)size.width
        && read_int(file,&depth) && depth==(uint32_t)size.depth
        && read_int(file,&height) && height==(uint32_t)size.height;
}

bool load_game(Game *g,const char *path,uint32_t seed,WorldSize size)
{
    if (!valid_world_size(size)) return false;
    FILE *file=open_save(path,false);
    if (!file) return false;
    Game *loaded=calloc(1,sizeof(*loaded));
    if (!loaded) { fclose(file); return false; }
    unsigned char header[8];
    uint32_t disk_seed,health,slot,creative;
    bool ok=fread(header,1,sizeof(header),file)==sizeof(header)
        && memcmp(header,magic,6)==0 && header[7]==0
        && header[6]>=1 && header[6]<=3
        && read_int(file,&disk_seed) && disk_seed==seed
        && (header[6]==1 || read_size(file,size))
        && read_int(file,&health) && health>0 && health<=20
        && read_int(file,&slot) && slot<SLOTS && read_int(file,&creative) && creative<=1;
    if (ok) {
        new_game(loaded,seed,size);
        ok=read_float(file,&loaded->pos.x) && read_float(file,&loaded->pos.y)
            && read_float(file,&loaded->pos.z) && read_float(file,&loaded->yaw)
            && read_float(file,&loaded->pitch) && read_float(file,&loaded->time);
        if (ok) {
            const Vec3 p=loaded->pos;
            const World *w=&loaded->world;
            ok=p.x>=w->min_x+.29f && p.x<=w->max_x-.29f &&
                p.z>=w->min_z+.29f && p.z<=w->max_z-.29f &&
                p.y>=.99f && p.y<size.height+4 && fabsf(loaded->pitch)<=1.55f &&
                loaded->time>=0 && loaded->time<1;
        }
        // Las versiones 1 y 2 numeraban los objetos antes de los bloques de D449: se traducen al cargar.
        const int stored=header[6]<3 ? OLD_ITEM_COUNT : ITEM_COUNT;
        for (int i=0; ok && i<stored; ++i) {
            uint32_t amount;
            ok=read_int(file,&amount) && amount<=999;
            if (ok) loaded->bag[header[6]<3 ? old_item_id(i) : i]=(uint16_t)amount;
        }
        ok=ok && fread(loaded->slots,1,SLOTS,file)==SLOTS;
        for (int i=0; ok && i<SLOTS; ++i) {
            if (header[6]<3) {
                ok=loaded->slots[i]<OLD_ITEM_COUNT;
                if (ok) loaded->slots[i]=(uint8_t)old_item_id(loaded->slots[i]);
            } else ok=loaded->slots[i]<ITEM_COUNT;
        }
        for (int i=0; ok && header[6]>=3 && i<ITEM_COUNT; ++i) {
            uint32_t uses;
            ok=read_int(file,&uses) && uses<=60000u;
            if (ok) loaded->wear[i]=(uint16_t)uses;
        }
        ok=ok && (header[6]==1 ? read_legacy_chunks(file,&loaded->world) :
            read_chunks(file,&loaded->world)) && fgetc(file)==EOF && !ferror(file);
    }
    if (ok) {
        if (body_blocked(&loaded->world,loaded->pos,.29f,1.75f)) {
            loaded->pos=loaded->world.start;
            while (loaded->pos.y<size.height &&
                body_blocked(&loaded->world,loaded->pos,.29f,1.75f)) loaded->pos.y+=1;
        }
        for (int i=0; i<10; ++i) {
            Mob *m=&loaded->mobs[i];
            if (m->active && body_blocked(&loaded->world,m->pos,.3f,1.7f))
                m->pos.y=surface_at(&loaded->world,(int)m->pos.x,(int)m->pos.z)+1.02f;
        }
        loaded->health=(int)health;
        loaded->selected=(int)slot;
        loaded->creative=creative!=0;
        refresh_wildlife(loaded,true);
        notify(loaded,header[6]==1 ? "Partida anterior ampliada" : "Mundo cargado");
        aim(loaded);
        ok=!loaded->world.failed;
    }
    if (ok) {
        close_world(&g->world);
        *g=*loaded;
    } else close_world(&loaded->world);
    fclose(file);
    free(loaded);
    return ok;
}
