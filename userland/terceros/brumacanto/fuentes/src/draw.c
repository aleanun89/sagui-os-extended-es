#include "draw.h"

#include <SDL.h>
#include "gl_api.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WHITE_TILE 63
#define RGBA(r,g,b,a) ((uint32_t)(r)<<24 | (uint32_t)(g)<<16 | (uint32_t)(b)<<8 | (uint32_t)(a))
#define RGB(r,g,b) RGBA(r,g,b,255)

typedef struct {
    GLfloat x,y,z,u,v;
    GLubyte r,g,b,a;
} Vertex;
typedef struct { Vertex *data; size_t count,capacity; } Batch;
typedef struct { GLuint ids[2]; GLsizei count[2]; uint64_t revision; } Mesh;
typedef struct {
    int x,z;
    uint8_t ceiling[CHUNK+2][CHUNK+2];
    uint8_t brightness[MAX_HEIGHT][CHUNK+2][CHUNK+2];
} Lighting;

static DrawGL gl;
static GLuint atlas;
static Mesh meshes[CACHE_SIDE][CACHE_SIDE];
static Batch opaque,transparent,dynamic;
static bool alloc_failed;

static const uint32_t white=RGB(247,247,230), muted=RGB(165,197,186);
static const uint32_t mint=RGB(124,230,184), gold=RGB(248,196,113);
static const uint32_t ink=RGB(18,38,39);
static const uint32_t panel=RGBA(15,32,33,247);

static const int normals[6][3]={
    {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}
};
static const int corners[6][4][3]={
    {{1,1,1},{1,0,1},{1,0,0},{1,1,0}},
    {{0,1,0},{0,0,0},{0,0,1},{0,1,1}},
    {{0,1,0},{0,1,1},{1,1,1},{1,1,0}},
    {{0,0,1},{0,0,0},{1,0,0},{1,0,1}},
    {{0,1,1},{0,0,1},{1,0,1},{1,1,1}},
    {{1,1,0},{1,0,0},{0,0,0},{0,1,0}}
};
static const float shade[6]={.78f,.87f,1,.53f,.90f,.72f};

static void vertex(Batch *batch, Vec3 p, float u, float v, uint32_t color)
{
    Vertex *vertex=&batch->data[batch->count++];
    *vertex=(Vertex){p.x,p.y,p.z,u,v,
        (GLubyte)(color>>24),(GLubyte)(color>>16),(GLubyte)(color>>8),(GLubyte)color};
}

static bool reserve(Batch *batch, size_t amount)
{
    if (batch->count+amount<=batch->capacity) return true;
    size_t n=batch->capacity ? batch->capacity*2 : 1024;
    while (n<batch->count+amount) n*=2;
    Vertex *data=realloc(batch->data,n*sizeof(*data));
    if (!data) { alloc_failed=true; return false; }
    batch->data=data;
    batch->capacity=n;
    return true;
}

static void quad(Batch *batch, const Vec3 p[4], int tile, const uint32_t colors[4])
{
    if (!reserve(batch,6)) return;
    const float u=(float)((tile%8)*64)+2.5f, v=(float)((tile/8)*64)+2.5f;
    const float uu[]={u,u,u+59,u+59},vv[]={v,v+59,v+59,v};
    static const int indices[]={0,1,2,0,2,3};
    for (int i=0; i<6; ++i) {
        int j=indices[i];
        vertex(batch,p[j],uu[j]/512,vv[j]/512,colors[j]);
    }
}

static void flat_quad(Batch *batch, const Vec3 p[4], int tile, uint32_t color)
{
    const uint32_t colors[]={color,color,color,color};
    quad(batch,p,tile,colors);
}

static uint32_t multiply(uint32_t color,float light)
{
    return RGBA((unsigned)fminf((float)(color>>24)*light,255),
        (unsigned)fminf((float)((color>>16)&255u)*light,255),
        (unsigned)fminf((float)((color>>8)&255u)*light,255),color&255u);
}

static void pointers(GLuint buffer,const Vertex *vertices)
{
    gl.BindBuffer(GL_ARRAY_BUFFER,buffer);
    const unsigned char *start=(const unsigned char *)vertices;
    gl.VertexPointer(3,GL_FLOAT,sizeof(Vertex),
        buffer ? (const void *)(uintptr_t)offsetof(Vertex,x) : start+offsetof(Vertex,x));
    gl.TexCoordPointer(2,GL_FLOAT,sizeof(Vertex),
        buffer ? (const void *)(uintptr_t)offsetof(Vertex,u) : start+offsetof(Vertex,u));
    gl.ColorPointer(4,GL_UNSIGNED_BYTE,sizeof(Vertex),
        buffer ? (const void *)(uintptr_t)offsetof(Vertex,r) : start+offsetof(Vertex,r));
}

static void flush(Batch *batch, GLenum mode)
{
    if (!batch->count) return;
    pointers(0,batch->data);
    gl.DrawArrays(mode,0,(GLsizei)batch->count);
    batch->count=0;
}

static char renderer_name[64];
bool draw_init(const char *asset_dir)
{
#define LOAD(type,name,params) do { \
        void *p=SDL_GL_GetProcAddress("gl" #name); \
        if (!p || sizeof(p)!=sizeof(gl.name)) { SDL_SetError("No disponible: gl" #name); return false; } \
        memcpy(&gl.name,&p,sizeof(p)); \
    } while (0);
    GL_FUNCTIONS(LOAD)
#undef LOAD
    const char *version=(const char *)gl.GetString(GL_VERSION);
    const char *renderer=(const char *)gl.GetString(GL_RENDERER);
    snprintf(renderer_name,sizeof(renderer_name),"%s",renderer ? renderer : "");
    SDL_Log("Renderizador: %s (%s)", renderer ? renderer : "?", version ? version : "?");
    if (!version || strncmp(version,"OpenGL ES-CM 1.1",16)!=0) {
        SDL_SetError("Se necesita un contexto OpenGL ES-CM 1.1.");
        return false;
    }

    const size_t n=512u*512u*4u;
    char file[1024];
    int length=snprintf(file,sizeof(file),"%satlas.rgba",asset_dir);
    if (length<0 || (size_t)length>=sizeof(file)) return false;
    SDL_RWops *src=SDL_RWFromFile(file,"rb");
    if (!src) { SDL_SetError("No se puede abrir %s",file); return false; }
    unsigned char *pixels=malloc(n);
    bool ok=pixels && SDL_RWsize(src)==(Sint64)n && SDL_RWread(src,pixels,1,n)==n;
    SDL_RWclose(src);
    if (!ok) { free(pixels); SDL_SetError("Atlas de texturas incompleto"); return false; }
    gl.GenTextures(1,&atlas);
    gl.BindTexture(GL_TEXTURE_2D,atlas);
    gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    gl.TexImage2D(GL_TEXTURE_2D,0,GL_RGBA,512,512,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    free(pixels);
    gl.Enable(GL_TEXTURE_2D);
    gl.EnableClientState(GL_VERTEX_ARRAY);
    gl.EnableClientState(GL_TEXTURE_COORD_ARRAY);
    gl.EnableClientState(GL_COLOR_ARRAY);
    gl.BlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    gl.AlphaFunc(GL_GREATER,.53f);
    return gl.GetError()==GL_NO_ERROR;
}

void draw_close(void)
{
    for (int z=0; z<CACHE_SIDE; ++z) for (int x=0; x<CACHE_SIDE; ++x)
        if (meshes[z][x].ids[0]) gl.DeleteBuffers(2,meshes[z][x].ids);
    if (atlas) gl.DeleteTextures(1,&atlas);
    free(opaque.data);
    free(transparent.data);
    free(dynamic.data);
}

static int view_distance=VIEW_DISTANCE_DEFAULT;
void draw_set_view_distance(int distance)
{
    view_distance=distance<VIEW_DISTANCE_MIN?VIEW_DISTANCE_MIN:distance>VIEW_DISTANCE_MAX?VIEW_DISTANCE_MAX:distance;
}
int draw_get_view_distance(void) { return view_distance; }
const char *draw_renderer(void) { return renderer_name; }

unsigned draw_error(void)
{
    return alloc_failed ? GL_OUT_OF_MEMORY : gl.GetError();
}

static bool occluder(World *w,int x,int y,int z)
{
    const int id=block_at(w,x,y,z);
    return id!=AIR && id!=LEAVES && id!=AUTUMN && id!=NEEDLES && blocks[id].opacity==255;
}

static void light_chunk(World *w,int cx,int cz,Lighting *light)
{
    memset(light,0,sizeof(*light));
    light->x=cx*CHUNK-1;
    light->z=cz*CHUNK-1;
    for (int z=0; z<CHUNK+2; ++z) for (int x=0; x<CHUNK+2; ++x) {
        for (int y=w->size.height-1; y>0; --y) {
            int id=block_at(w,light->x+x,y,light->z+z);
            if (id!=LANTERN && id!=LOG && id!=BIRCH && id!=PINE_LOG && occluder(w,light->x+x,y,light->z+z)) {
                light->ceiling[z][x]=(uint8_t)y;
                break;
            }
        }
    }
    const int r=LAMP_RADIUS;
    for (int y=1; y<w->size.height; ++y) for (int z=-r; z<CHUNK+2+r; ++z) {
        for (int x=-r; x<CHUNK+2+r; ++x) {
            if (!emits_light(block_at(w,light->x+x,y,light->z+z))) continue;
            for (int yy=y-r; yy<=y+r; ++yy) for (int zz=z-r; zz<=z+r; ++zz) {
                for (int xx=x-r; xx<=x+r; ++xx) {
                    if ((unsigned)xx>=CHUNK+2 || (unsigned)zz>=CHUNK+2 || (unsigned)yy>=(unsigned)w->size.height) continue;
                    int distance=abs(xx-x)+abs(yy-y)+abs(zz-z);
                    if (distance>r) continue;
                    uint8_t value=(uint8_t)((r+1-distance)*35);
                    if (value>light->brightness[yy][zz][xx]) light->brightness[yy][zz][xx]=value;
                }
            }
        }
    }
}

static float corner_light(World *w,int x,int y,int z,int face,int i)
{
    int a[3]={0,0,0},b[3]={0,0,0};
    const int *n=normals[face],*c=corners[face][i];
    int axis=0;
    for (int k=0; k<3; ++k) if (!n[k]) {
        (axis++==0 ? a : b)[k]=c[k]*2-1;
    }
    x+=n[0]; y+=n[1]; z+=n[2];
    bool p=occluder(w,x+a[0],y+a[1],z+a[2]);
    bool q=occluder(w,x+b[0],y+b[1],z+b[2]);
    bool r=occluder(w,x+a[0]+b[0],y+a[1]+b[1],z+a[2]+b[2]);
    int darkness=p&&q ? 3 : p+q+r;
    return 1-.12f*(float)darkness;
}

static void voxel_face(World *w,const Lighting *lighting,int x,int y,int z,int id,int side)
{
    const Block *b=&blocks[id];
    Vec3 p[4];
    uint32_t colors[4];
    const int tile=side==F_UP ? b->top : (side==F_DOWN ? b->bottom : b->side);
    const float variation=.91f+.09f*(float)(terrain_hash(w->seed,x,y,z)%8u)/7;
    const int lx=x+normals[side][0]-lighting->x,lz=z+normals[side][2]-lighting->z;
    const int ly=y+normals[side][1];
    const float underground=clampf((lighting->ceiling[lz][lx]-ly-1)*.13f,0,.58f);
    const float lamp=(unsigned)ly<(unsigned)w->size.height ? lighting->brightness[ly][lz][lx]/210.0f : 0;
    for (int i=0; i<4; ++i) {
        const int *c=corners[side][i];
        float surface=(float)c[1];
        if (surface>0 && (id==WATER || id==LAVA) && block_at(w,x,y+1,z)!=id) surface=.87f;
        p[i]=(Vec3){(float)(x+c[0]),(float)y+surface,(float)(z+c[2])};
        float light=shade[side]*variation*corner_light(w,x,y,z,side,i)*(1-underground);
        light=fmaxf(light,lamp*.97f);
        if (id==LANTERN || id==LAVA) light=1.0f;
        const uint32_t base=id==CLAY?RGBA(185,209,222,b->opacity):
            RGBA(255,(unsigned)(255-lamp*9),(unsigned)(255-lamp*21),b->opacity);
        colors[i]=multiply(base,light);
    }
    quad(b->opacity<255 ? &transparent : &opaque,p,tile,colors);
}

static void grass_detail(World *w,int x,int y,int z)
{
    const uint32_t variation=terrain_hash(w->seed,x,y,z);
    if (variation%12u>1u) return;
    float X=(float)x,Y=(float)y+.997f,Z=(float)z;
    float h=.36f+(float)((variation>>8)&15u)*.019f;
    const int tile=variation%41u==0 ? 41 : (variation%37u==0 ? 53 : (38+(int)(variation%3u)));
    const Vec3 p[]={ {X+.12f,Y+h,Z+.12f},{X+.12f,Y,Z+.12f},
                      {X+.88f,Y,Z+.88f},{X+.88f,Y+h,Z+.88f} };
    const Vec3 q[]={ {X+.12f,Y+h,Z+.88f},{X+.12f,Y,Z+.88f},
                      {X+.88f,Y,Z+.12f},{X+.88f,Y+h,Z+.12f} };
    const Vec3 rp[]={p[3],p[2],p[1],p[0]}, rq[]={q[3],q[2],q[1],q[0]};
    flat_quad(&opaque,p,tile,white); flat_quad(&opaque,q,tile,white);
    flat_quad(&opaque,rp,tile,white); flat_quad(&opaque,rq,tile,white);
}

static bool exposed(int id, int other)
{
    if (other==AIR) return true;
    if (id==other) return false;
    if (id==WATER || id==LAVA) return false;
    if (id==GLASS) return other==WATER;
    return blocks[other].opacity<255;
}

static void mesh_chunk(World *w,const WorldChunk *chunk,Mesh *m)
{
    const int cx=chunk->x,cz=chunk->z;
    opaque.count=transparent.count=0;
    Lighting lighting;
    light_chunk(w,cx,cz,&lighting);
    for (int y=0; y<w->size.height; ++y) for (int z=0; z<CHUNK; ++z) {
        for (int x=0; x<CHUNK; ++x) {
            const int id=chunk->cells[chunk_offset(x,y,z)];
            if (id==AIR) continue;
            const int wx=cx*CHUNK+x,wz=cz*CHUNK+z;
            for (int f=0; f<6; ++f) {
                const int xx=wx+normals[f][0],yy=y+normals[f][1],zz=wz+normals[f][2];
                if (yy<0 || !exposed(id,block_at(w,xx,yy,zz))) continue;
                voxel_face(w,&lighting,wx,y,wz,id,f);
            }
            if (id==GRASS && block_at(w,wx,y+1,wz)==AIR) grass_detail(w,wx,y+1,wz);
        }
    }
    if (!m->ids[0]) gl.GenBuffers(2,m->ids);
    const Batch *meshed[2]={&opaque,&transparent};
    for (int i=0; i<2; ++i) {
        m->count[i]=(GLsizei)meshed[i]->count;
        gl.BindBuffer(GL_ARRAY_BUFFER,m->ids[i]);
        gl.BufferData(GL_ARRAY_BUFFER,(GLsizeiptr)(meshed[i]->count*sizeof(Vertex)),meshed[i]->data,GL_STATIC_DRAW);
    }
    m->revision=chunk->revision;
}

static void box(Batch *batch,Vec3 p,Vec3 size,float yaw,uint32_t color)
{
    const float s=sinf(yaw),c=cosf(yaw);
    for (int f=0; f<6; ++f) {
        Vec3 v[4];
        for (int i=0; i<4; ++i) {
            const int *corner=corners[f][i];
            const float x=(float)corner[0]*size.x+p.x;
            const float y=(float)corner[1]*size.y+p.y;
            const float z=(float)corner[2]*size.z+p.z;
            v[i]=(Vec3){x*c-z*s,y,x*s+z*c};
        }
        flat_quad(batch,v,WHITE_TILE,multiply(color,shade[f]));
    }
}

static void part(Vec3 offset,Vec3 size,float yaw,uint32_t color)
{
    box(&dynamic,offset,size,yaw,color);
}

static void mob_draw(const Mob *m,float time)
{
    const float yaw=m->yaw;
    const uint32_t flash=m->hurt>0 ? RGB(255,122,104) : RGB(255,255,255);
    const float stride=sinf(m->phase);
    if (m->kind<VILLAGER) {
        const bool hare=m->kind==HARE, fox=m->kind==FOX;
        const float r=hare?.20f:(fox?.23f:.31f);
        const float h=hare?.27f:(fox?.36f:.49f);
        const uint32_t body=m->hurt>0 ? flash : (hare?RGB(188,168,151):(fox?RGB(217,102,46):RGB(104,74,64)));
        const uint32_t accent=hare?RGB(235,212,192):(fox?RGB(250,220,155):RGB(169,111,100));
        const float length=hare?.54f:(fox?.85f:.92f);
        part((Vec3){-r,.17f,-length*.53f},(Vec3){r*2,h,length},yaw,body);
        part((Vec3){-r*.82f,.25f+h*.60f,-length*.69f},(Vec3){r*1.64f,h*.78f,.31f},yaw,body);
        part((Vec3){-r*.53f,.26f+h*.68f,-length*.75f-.12f},(Vec3){r*1.06f,.14f,.20f},yaw,accent);
        for (int i=-1; i<=1; i+=2) {
            part((Vec3){i*r*.56f-.04f,.25f+h*1.33f,-length*.45f},(Vec3){.09f,hare?.21f:.13f,.09f},yaw,body);
            part((Vec3){i*r*.61f-.035f,.30f+h,-length*.70f-.013f},(Vec3){.07f,.069f,.04f},yaw,ink);
            for (int j=-1; j<=1; j+=2)
                part((Vec3){i*(r-.08f)-.06f,0,j*length*.31f-.06f+stride*i*j*.07f},
                     (Vec3){.12f,.23f,.14f},yaw,accent);
        }
        if (fox || hare) {
            part((Vec3){-.10f,.23f,length*.34f},(Vec3){.2f,.17f,fox?.44f:.18f},yaw,body);
            part((Vec3){-.10f,.25f,length*.34f+(fox?.31f:.10f)},(Vec3){.2f,.14f,.13f},yaw,accent);
        }
        return;
    }
    if (m->kind==WISP) {
        const float lift=.38f+.16f*sinf(time*6+m->phase);
        const uint32_t shell=m->hurt>0?flash:RGB(54,134,143);
        part((Vec3){-.22f,lift,-.22f},(Vec3){.44f,.44f,.44f},yaw,shell);
        part((Vec3){-.13f,lift+.12f,-.24f},(Vec3){.26f,.18f,.05f},yaw,RGB(135,253,223));
        part((Vec3){-.3f,lift+.44f,-.30f},(Vec3){.6f,.08f,.6f},yaw,RGB(75,81,124));
        for (int i=-1; i<=1; i+=2) {
            part((Vec3){i*.36f-.075f,lift+.17f,-.09f},(Vec3){.15f,.13f,.18f},yaw,mint);
            part((Vec3){i*.16f-.04f,lift-.14f,.04f},(Vec3){.08f,.08f,.08f},yaw,mint);
        }
        return;
    }
    const bool villager=m->kind==VILLAGER, golem=m->kind==GOLEM;
    const float scale=golem?1.16f:1.0f;
    uint32_t clothes=villager?RGB(88,134,108):(golem?RGB(107,118,105):RGB(81,74,120));
    const uint32_t face=villager?RGB(218,160,114):(golem?RGB(146,145,119):RGB(129,114,173));
    if (m->hurt>0) clothes=flash;
    for (int i=-1; i<=1; i+=2) {
        const float swing=stride*(float)i*.13f;
        part((Vec3){i*.123f*scale-.09f,0,-.103f+swing},(Vec3){.18f*scale,.51f*scale,.23f},yaw,RGB(62,67,78));
        part((Vec3){i*.285f*scale-.075f,.50f*scale,-.13f-swing},(Vec3){.15f*scale,.66f*scale,.24f},yaw,clothes);
        part((Vec3){i*.285f*scale-.075f,.49f*scale,-.135f-swing},(Vec3){.15f*scale,.14f*scale,.25f},yaw,face);
    }
    part((Vec3){-.235f*scale,.46f*scale,-.17f},(Vec3){.47f*scale,.69f*scale,.34f},yaw,clothes);
    part((Vec3){-.237f*scale,.65f*scale,-.177f},(Vec3){.474f*scale,.09f,.354f},yaw,villager?RGB(123,77,50):ink);
    part((Vec3){-.215f*scale,1.12f*scale,-.216f},(Vec3){.43f*scale,.46f*scale,.43f},yaw,face);
    if (villager) {
        part((Vec3){-.32f,1.48f,-.30f},(Vec3){.64f,.10f,.60f},yaw,gold);
        part((Vec3){-.23f,1.57f,-.22f},(Vec3){.46f,.13f,.43f},yaw,RGB(179,126,67));
        part((Vec3){-.05f,1.24f,-.26f},(Vec3){.10f,.09f,.075f},yaw,RGB(184,119,76));
    } else {
        part((Vec3){-.245f*scale,1.49f*scale,-.24f},(Vec3){.49f*scale,.13f,.48f},yaw,clothes);
    }
    const uint32_t eye=villager?ink:(golem?gold:RGB(204,149,248));
    for (int i=-1; i<=1; i+=2) {
        part((Vec3){i*.11f*scale-.04f,1.36f*scale,-.229f},(Vec3){.08f,.065f,.035f},yaw,eye);
    }
}

static void rect_color(float x,float y,float w,float h,uint32_t top,uint32_t bottom)
{
    const Vec3 p[]={{x,y,0},{x,y+h,0},{x+w,y+h,0},{x+w,y,0}};
    const uint32_t colors[]={top,bottom,bottom,top};
    quad(&dynamic,p,WHITE_TILE,colors);
}

static void rect(float x,float y,float w,float h,uint32_t c)
{
    rect_color(x,y,w,h,c,c);
}

static void tile_rect(float x,float y,float w,float h,int tile,uint32_t c)
{
    const Vec3 p[]={{x,y,0},{x,y+h,0},{x+w,y+h,0},{x+w,y,0}};
    flat_quad(&dynamic,p,tile,c);
}

static void framed_panel(float x,float y,float w,float h,uint32_t color,uint32_t border)
{
    rect(x+3,y+4,w,h,RGBA(2,12,18,120));
    rect(x,y,w,h,border);
    rect(x+1,y+1,w-2,h-2,color);
}

static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-+:/.,!?[]%_=<> ";
static const uint8_t glyphs[][7]={
    {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
    {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
    {14,17,16,23,17,17,14},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
    {7,2,2,2,2,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
    {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
    {17,17,17,17,17,10,4},{17,17,17,21,21,27,17},{17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
    {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14},
    {0,0,0,31,0,0,0},{0,4,4,31,4,4,0},{0,4,4,0,4,4,0},
    {1,1,2,4,8,16,16},{0,0,0,0,0,12,12},{0,0,0,0,0,12,8},
    {4,4,4,4,4,0,4},{14,17,1,2,4,0,4},
    {14,8,8,8,8,8,14},{14,2,2,2,2,2,14},{17,2,4,8,17,0,0},
    {0,0,0,0,0,0,31},{0,31,0,31,0,0,0},{2,4,8,16,8,4,2},
    {8,4,2,1,2,4,8},{0,0,0,0,0,0,0}
};

static void text_at(float x,float y,float size,uint32_t color,const char *string)
{
    for (const unsigned char *p=(const unsigned char *)string; *p; ++p,x+=size*6) {
        const char *letter=strchr(alphabet,toupper(*p));
        const uint8_t *rows=glyphs[letter ? letter-alphabet : 41];
        for (int row=0; row<7; ++row) {
            for (int col=0; col<5; ++col) {
                if (!(rows[row]&(16>>col))) continue;
                const int from=col;
                while (col<4 && (rows[row]&(16>>(col+1)))) ++col;
                rect(x+(float)from*size,y+(float)row*size,(float)(col-from+1)*size,size,color);
            }
        }
    }
}

static float text_width(float size,const char *s)
{
    return (float)strlen(s)*size*6-size;
}

static void text_center(float x,float y,float size,uint32_t color,const char *s)
{
    text_at(x-text_width(size,s)*.5f,y,size,color,s);
}

static void text_shadow(float x,float y,float size,uint32_t color,const char *s)
{
    text_at(x+1.3f,y+1.5f,size,RGBA(8,23,28,220),s);
    text_at(x,y,size,color,s);
}

static float ui_scale(int width,int height)
{
    return fminf((float)width/UI_W,(float)height/UI_H);
}

void ui_pointer(int w,int h,int x,int y,float *ux,float *uy)
{
    const float scale=ui_scale(w,h);
    *ux=((float)x-(float)w*.5f)/scale+UI_W*.5f;
    *uy=((float)y-(float)h*.5f)/scale+UI_H*.5f;
}

static void set_ui(int width,int height)
{
    float x=(float)width/(2*ui_scale(width,height));
    float y=(float)height/(2*ui_scale(width,height));
    gl.Disable(GL_DEPTH_TEST);
    gl.Disable(GL_FOG);
    gl.Disable(GL_CULL_FACE);
    gl.Disable(GL_ALPHA_TEST);
    gl.Enable(GL_BLEND);
    gl.MatrixMode(GL_PROJECTION);
    gl.LoadIdentity();
    gl.Orthof(UI_W*.5f-x,UI_W*.5f+x,UI_H*.5f+y,UI_H*.5f-y,-1,1);
    gl.MatrixMode(GL_MODELVIEW);
    gl.LoadIdentity();
}

static uint32_t blend_rgb(uint32_t a,uint32_t b,float t)
{
    const int r=(int)((1-t)*(float)(a>>24)+t*(float)(b>>24));
    const int g=(int)((1-t)*(float)((a>>16)&255u)+t*(float)((b>>16)&255u));
    const int bl=(int)((1-t)*(float)((a>>8)&255u)+t*(float)((b>>8)&255u));
    return RGB(r,g,bl);
}

static void sky_detail(Vec3 eye,float yaw,float time,float day)
{
    const float x=eye.x,z=eye.z;
    const float sun_angle=time*2*PI;
    const float sy=sinf(sun_angle),cx=cosf(sun_angle);
    const Vec3 orb={x+cx*66,eye.y+fabsf(sy)*52,z-54};
    const float rad=sy>=0?5.0f:3.3f;
    // Camera-facing vertical billboards stay in the same celestial direction.
    const float s=sinf(yaw)*rad,c=cosf(yaw)*rad;
    const Vec3 face[]={{orb.x-c,orb.y+rad,orb.z-s},{orb.x-c,orb.y-rad,orb.z-s},
        {orb.x+c,orb.y-rad,orb.z+s},{orb.x+c,orb.y+rad,orb.z+s}};
    flat_quad(&dynamic,face,sy>=0?36:37,white);
    for (int i=0; i<12; ++i) {
        const uint32_t h=terrain_hash(529,(int)floorf(x/16),i,(int)floorf(z/16));
        const float px=x+(float)((int)(h&127u)-64),pz=z+(float)((int)((h>>7)&127u)-64);
        const float py=fmaxf(35.0f,eye.y+8.0f)+(float)((h>>14)&15u);
        const float w=6.0f+(float)((h>>18)&15u);
        const uint32_t cloud=RGBA((int)(114+135*day),(int)(137+110*day),(int)(176+72*day),190);
        box(&dynamic,(Vec3){px,py,pz},(Vec3){w,.40f,3.3f},0,cloud);
        box(&dynamic,(Vec3){px+2,py+.08f,pz+3.28f},(Vec3){w*.66f,.35f,2.4f},0,cloud);
    }
    flush(&dynamic,GL_TRIANGLES);
}

static void outline(const Hit *h)
{
    if (!reserve(&dynamic,24)) return;
    const float low=-.004f,high=1.004f;
    Vec3 p[8];
    for (int i=0; i<8; ++i) p[i]=(Vec3){
        (float)h->x+(i&1?high:low),(float)h->y+(i&2?high:low),(float)h->z+(i&4?high:low)};
    const float uv=480.0f/512;
    for (int i=0; i<8; ++i) for (int axis=1; axis<8; axis*=2) {
        if (i&axis) continue;
        vertex(&dynamic,p[i],uv,uv,RGB(243,245,199));
        vertex(&dynamic,p[i|axis],uv,uv,RGB(243,245,199));
    }
    flush(&dynamic,GL_LINES);
}

typedef struct { const Mesh *mesh; float distance; } Visible;

static int far_first(const void *left,const void *right)
{
    const float a=((const Visible *)left)->distance,b=((const Visible *)right)->distance;
    return a>b ? -1 : (a<b ? 1 : 0);
}

static void world_view(Game *g,const View *view,int width,int height)
{
    const int VIEW_DISTANCE=view_distance;
    const bool title=view->screen==MENU;
    const float yaw=title?.20f+.095f*sinf(view->animation*.27f):g->yaw;
    const float pitch=title?-.14f:g->pitch;
    const Vec3 eye=title?(Vec3){76.9f,24.7f,101.7f}:
        (Vec3){g->pos.x,g->pos.y+1.59f,g->pos.z};
    const bool underwater=!title && block_at(&g->world,(int)floorf(eye.x),(int)eye.y,
        (int)floorf(eye.z))==WATER;
    const float day=title?1.0f:daylight(g);
    const float time=title?.22f:g->time;
    const uint32_t horizon=underwater?RGB(64,125,143):blend_rgb(RGB(65,77,108),RGB(192,222,207),day);
    const uint32_t upper=blend_rgb(RGB(23,35,67),RGB(77,159,196),day);
    gl.Viewport(0,0,width,height);
    gl.ClearColor((float)(horizon>>24)/255,(float)((horizon>>16)&255u)/255,
        (float)((horizon>>8)&255u)/255,1);
    gl.Clear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    set_ui(width,height);
    rect_color(-3000,-1000,6960,1300,upper,upper);
    rect_color(-3000,-20,6960,380,upper,horizon);
    flush(&dynamic,GL_TRIANGLES);
    gl.MatrixMode(GL_PROJECTION);
    gl.LoadIdentity();
    const float top=.051f,ratio=(float)width/(float)height;
    gl.Frustumf(-top*ratio,top*ratio,-top,top,.075f,135);
    gl.MatrixMode(GL_MODELVIEW);
    gl.LoadIdentity();
    gl.Rotatef(-pitch*180/PI,1,0,0);
    gl.Rotatef(yaw*180/PI,0,1,0);
    gl.Translatef(-eye.x,-eye.y,-eye.z);
    gl.Enable(GL_DEPTH_TEST);
    gl.DepthMask(GL_FALSE);
    if (!underwater) sky_detail(eye,yaw,time,day);
    gl.DepthMask(GL_TRUE);
    gl.Disable(GL_BLEND);
    gl.Enable(GL_CULL_FACE);
    gl.Enable(GL_ALPHA_TEST);
    gl.Enable(GL_FOG);
    const GLfloat fog[]={(float)(horizon>>24)/255,(float)((horizon>>16)&255u)/255,
        (float)((horizon>>8)&255u)/255,1};
    gl.Fogf(GL_FOG_MODE,GL_LINEAR);
    gl.Fogf(GL_FOG_START,underwater?1:fminf(20+day*11,(float)VIEW_DISTANCE*.4f));
    gl.Fogf(GL_FOG_END,underwater?14:fminf(92.f,(float)VIEW_DISTANCE-4.f));
    gl.Fogfv(GL_FOG_COLOR,fog);

    int count=0;
    /* SaguiOS (D444): la distancia de dibujo es ajustable (draw_set_view_distance); el máximo (104) es el del original. */
    _Static_assert(2*(VIEW_DISTANCE_MAX+CHUNK/2+LAMP_RADIUS+1)<CACHE_SIDE*CHUNK,
        "Visible sectors and lighting neighbors must fit in the mesh cache");
    Visible visible[CACHE_SIDE*CACHE_SIDE];
    const float sx=sinf(yaw),cz=-cosf(yaw);
    const int min_x=chunk_coordinate((int)floorf(eye.x)-VIEW_DISTANCE);
    const int min_z=chunk_coordinate((int)floorf(eye.z)-VIEW_DISTANCE);
    const int max_x=chunk_coordinate((int)floorf(eye.x)+VIEW_DISTANCE);
    const int max_z=chunk_coordinate((int)floorf(eye.z)+VIEW_DISTANCE);
    for (int z=min_z; z<=max_z; ++z) for (int x=min_x; x<=max_x; ++x) {
        const float a=(float)(x*CHUNK+CHUNK/2)-eye.x,b=(float)(z*CHUNK+CHUNK/2)-eye.z;
        const float distance=a*a+b*b;
        if (distance>VIEW_DISTANCE*VIEW_DISTANCE || (a*sx+b*cz < -20 && fabsf(pitch)<1.05f)) continue;
        const WorldChunk *chunk=world_chunk(&g->world,x,z);
        if (!chunk) continue;
        Mesh *m=&meshes[cache_coordinate(z)][cache_coordinate(x)];
        if (m->revision!=chunk->revision) mesh_chunk(&g->world,chunk,m);
        visible[count++]=(Visible){m,distance};
        if (m->count[0]) {
            pointers(m->ids[0],NULL);
            gl.DrawArrays(GL_TRIANGLES,0,m->count[0]);
        }
    }

    for (int i=0; i<MAX_MOBS; ++i) {
        Mob *m=&g->mobs[i];
        const float dx=eye.x-m->pos.x,dz=eye.z-m->pos.z;
        if (!m->active || dx*dx+dz*dz>64*64) continue;
        const size_t start=dynamic.count;
        mob_draw(m,view->animation);
        for (size_t n=start; n<dynamic.count; ++n) {
            dynamic.data[n].x+=m->pos.x;
            dynamic.data[n].y+=m->pos.y;
            dynamic.data[n].z+=m->pos.z;
        }
    }
    for (int i=0; i<MAX_BOLTS; ++i) {
        const Bolt *b=&g->bolts[i];
        if (b->lifetime>0) box(&dynamic,b->pos,(Vec3){.15f,.15f,.15f},0,mint);
    }
    flush(&dynamic,GL_TRIANGLES);

    gl.Disable(GL_ALPHA_TEST);
    gl.Disable(GL_CULL_FACE);
    gl.Enable(GL_BLEND);
    gl.DepthMask(GL_FALSE);
    qsort(visible,(size_t)count,sizeof(*visible),far_first);
    for (int i=0; i<count; ++i) {
        const Mesh *m=visible[i].mesh;
        if (m->count[1]) {
            pointers(m->ids[1],NULL);
            gl.DrawArrays(GL_TRIANGLES,0,m->count[1]);
        }
    }
    if (view->screen==PLAY && g->mob_target<0 && g->aimed.found) outline(&g->aimed);
    gl.DepthMask(GL_TRUE);
    gl.Disable(GL_FOG);
    set_ui(width,height);
    if (day<.99f) rect(-3000,-3000,6960,6540,RGBA(18,27,67,(unsigned)((1-day)*154)));
    if (underwater) rect(-3000,-3000,6960,6540,RGBA(14,86,123,71));
    if (g->hurt>0 && view->screen==PLAY) {
        rect(-3000,-3000,6960,6540,RGBA(151,30,40,(unsigned)(g->hurt*66)));
    }
    flush(&dynamic,GL_TRIANGLES);
}

static void button(float x,float y,float width,const char *label,bool active)
{
    framed_panel(x,y,width,35,active?RGB(53,105,88):RGB(29,54,54),
        active?mint:RGB(68,105,101));
    if (active) rect(x,y,4,35,mint);
    text_at(x+15,y+11,1.8f,active?white:muted,label);
}

static void menu_field(float x,float y,float width,const char *value,bool focus,float animation)
{
    framed_panel(x,y,width,FIELD_HEIGHT,ink,focus?mint:RGB(75,121,114));
    text_at(x+12,y+9,1.65f,white,value);
    if (focus && (int)(animation*2)%2==0)
        rect(x+15+fmaxf(0,text_width(1.65f,value)),y+8,2,15,mint);
}

static void menu(const View *view)
{
    rect_color(-3000,-3000,6960,6540,RGBA(18,36,40,42),RGBA(12,29,33,113));
    rect(0,-3000,425,6540,RGBA(12,27,31,237));
    rect(424,-3000,1,6540,RGBA(122,202,167,145));
    rect(45,62,33,2,mint);
    text_at(89,59,1.2f,mint,"EXPLORACION Y CONSTRUCCION");
    text_shadow(43,103,5.1f,white,"BRUMACANTO");
    text_at(46,159,1.55f,muted,"Explora, excava y construye.");
    text_at(46,179,1.55f,muted,"Vuelve a la aldea al caer la noche.");

    text_at(46,204,1.32f,mint,"SEMILLA DEL MUNDO");
    menu_field(MENU_X,SEED_Y,MENU_BUTTON_W,view->seed_text,view->input_field==0,view->animation);
    static const char *const names[]={"ANCHURA X","FONDO Z","ALTURA Y"};
    for (int i=0; i<3; ++i) {
        const float x=MENU_X+(float)(i*SIZE_STEP);
        text_at(x+1,267,1.28f,mint,names[i]);
        menu_field(x,SIZE_Y,SIZE_WIDTH,view->dimensions[i],view->input_field==i+1,view->animation);
    }
    text_at(46,326,1.14f,view->valid_size?muted:gold,
        view->valid_size?"TAB / CLIC  EDITAR TAMANO":"X/Z: 160-32768 DE 32 EN 32. Y: 48-128.");
    button(MENU_X,MENU_Y,MENU_BUTTON_W,view->existing?"CONTINUAR MUNDO":"EXPLORAR MUNDO",view->menu_cursor==0);
    button(MENU_X,MENU_Y+MENU_STEP,MENU_BUTTON_W,"SEMILLA ALEATORIA",view->menu_cursor==1);
    button(MENU_X,MENU_Y+2*MENU_STEP,MENU_BUTTON_W,"SALIR",view->menu_cursor==2);
    text_at(45,500,1.25f,muted,"ENTER PARA JUGAR    ESC PARA SALIR");

    framed_panel(635,452,285,55,RGBA(14,34,38,218),RGBA(151,213,186,164));
    text_at(652,464,1.38f,mint,"ALDEA DE BRUMA");
    text_at(652,484,1.25f,white,"WASD  MOVERSE   E  MOCHILA");
}

static void hud(Game *g,const View *v)
{
    const float a=18.0f;
    char buffer[128];
    framed_panel(a,a,225,49,RGBA(15,32,33,222),RGBA(114,164,151,150));
    text_at(29,28,1.45f,white,"BRUMACANTO");
    const int minute=((int)(g->time*1440)+360)%1440;
    snprintf(buffer,sizeof(buffer),"%02d:%02d   %s",minute/60,minute%60,
        g->creative?"CREATIVO":"SUPERVIVENCIA");
    text_at(29,48,1.22f,g->creative?gold:mint,buffer);
    snprintf(buffer,sizeof(buffer),"X %d   Y %d   Z %d",(int)floorf(g->pos.x),
        (int)floorf(g->pos.y+.002f),(int)floorf(g->pos.z));
    float place=text_width(1.28f,buffer);
    framed_panel(UI_W-18-place-19,18,place+19,24,RGBA(15,32,33,210),RGBA(114,164,151,135));
    text_at(UI_W-27-place,25,1.28f,muted,buffer);

    if (g->notice_time>0) {
        const float w=text_width(1.32f,g->notice)+20;
        framed_panel(UI_W*.5f-w*.5f,79,w,26,RGBA(15,32,33,225),RGBA(114,164,151,150));
        text_center(UI_W*.5f,87,1.32f,white,g->notice);
    }

    const float start=UI_W*.5f-225;
    rect(start+1,465,165,11,RGBA(14,28,32,228));
    for (int i=0; i<10; ++i) {
        const float x=start+4+(float)i*16;
        uint32_t color=g->creative ? mint : (g->health>=2*i+2?RGB(230,102,100):
            (g->health==2*i+1?RGB(169,76,81):RGB(84,55,66)));
        rect(x,468,13,5,color);
    }
    const int item=g->slots[g->selected];
    if (item!=AIR) {
        snprintf(buffer,sizeof(buffer),"%s%s",item_name(item),!g->creative && !g->bag[item]?" / AGOTADO":"");
        const float length=text_width(1.26f,buffer);
        rect(start+450-length-14,462,length+14,19,RGBA(14,28,32,202));
        text_at(start+450-length-7,467,1.26f,white,buffer);
    }
    for (int i=0; i<SLOTS; ++i) {
        const float x=start+(float)i*50;
        const bool selected=i==g->selected;
        framed_panel(x,483,46,42,selected?RGBA(54,82,66,252):RGBA(19,36,39,232),
            selected?mint:RGBA(130,177,160,175));
        if (selected) rect(x+1,484,44,2,mint);
        snprintf(buffer,sizeof(buffer),"%d",i+1);
        text_at(x+4,489,1.04f,selected?gold:muted,buffer);
        int id=g->slots[i];
        if (id>AIR) {
            const bool owned=g->creative || g->bag[id]>0;
            tile_rect(x+11,488,27,27,item_icons[id],owned?white:RGBA(143,160,166,120));
            const int life=tool_life(id);
            if (life && owned && !g->creative) {   // barra de desgaste de la herramienta
                const float left=1.0f-(float)g->wear[id]/(float)life;
                rect(x+8,517,30,3,RGBA(13,27,31,220));
                rect(x+8,517,30*left,3,left>.5f?mint:(left>.2f?gold:RGB(230,102,100)));
            }
            if (id<BLOCK_COUNT || id==BERRIES || id==INGOT_COPPER || id==INGOT_IRON || id==GEM) {
                if (g->creative) snprintf(buffer,sizeof(buffer),"INF");
                else snprintf(buffer,sizeof(buffer),"%u",(unsigned)g->bag[id]);
                text_shadow(x+43-text_width(1.05f,buffer),515,1.05f,white,buffer);
            }
        }
    }
    text_shadow(20,491,1.19f,white,"E  MOCHILA");
    text_shadow(20,509,1.19f,white,"F5  GUARDAR");
    text_shadow(747,491,1.19f,muted,"F2  CAMBIAR MODO");
    text_shadow(747,509,1.19f,muted,"ESC  PAUSA");

    if (v->screen!=PLAY) return;
    const uint32_t center=g->mob_target>=0?gold:white;
    rect(471,269,6,2,RGBA(22,34,37,209)); rect(483,269,7,2,RGBA(22,34,37,209));
    rect(479,261,2,6,RGBA(22,34,37,209)); rect(479,273,2,7,RGBA(22,34,37,209));
    rect(471,268,6,1,center); rect(483,268,6,1,center);
    rect(479,261,1,6,center); rect(479,273,1,6,center);
    if (g->mob_target>=0 || g->aimed.found) {
        const char *name=g->mob_target>=0?mob_names[g->mobs[g->mob_target].kind]:
            item_name(block_at(&g->world,g->aimed.x,g->aimed.y,g->aimed.z));
        const float w=text_width(1.18f,name)+16;
        rect(UI_W*.5f-w*.5f,289,w,18,RGBA(13,27,31,174));
        text_center(UI_W*.5f,294,1.18f,center,name);
        if (g->mob_target>=0 && g->mobs[g->mob_target].kind==VILLAGER)
            text_center(UI_W*.5f,313,1.18f,white,"F  COMERCIAR");
    }
    if (g->mining>0) {
        rect(441,319,78,6,RGBA(13,27,31,198));
        rect(443,321,74*g->mining,2,mint);
    }
    if (tool_kind(item) && (g->creative || g->bag[item])) {
        const float swing=5.5f*sinf(g->mining*36);
        tile_rect(775+swing,340+swing,142,142,item_icons[item],RGBA(243,238,222,160));
    }
}

static void bag_ui(Game *g,const View *v)
{
    framed_panel(21,33,918,474,panel,RGB(90,142,126));
    rect(21,33,918,3,mint);
    text_at(49,59,3.2f,white,"MOCHILA");
    text_at(49,105,1.23f,muted,"FLECHAS O RATON  /  1-9 ASIGNAR A LA BARRA");
    rect(RECIPE_X-18,103,1,373,RGB(62,96,91));
    for (int id=1; id<ITEM_COUNT; ++id) {
        int index=id-1;
        const float x=BAG_X+(float)(index%BAG_COLUMNS)*BAG_STEP;
        const float y=BAG_Y+(float)(index/BAG_COLUMNS)*BAG_STEP;
        const bool active=!v->crafting && id==v->item_cursor;
        const bool has=g->creative || g->bag[id]>0;
        framed_panel(x,y,42,41,active?RGB(53,99,86):RGB(31,54,55),
            active?mint:RGB(65,99,95));
        tile_rect(x+6,y+3,29,29,item_icons[id],has?white:RGBA(124,147,145,122));
        if (has) {
            char n[10];
            if (g->creative) snprintf(n,sizeof(n),"INF");
            else snprintf(n,sizeof(n),"%u",(unsigned)g->bag[id]);
            text_shadow(x+39-text_width(1.03f,n),y+31,1.03f,white,n);
        }
    }
    text_at(49,478,1.4f,mint,item_name(v->item_cursor));
    text_at(RECIPE_X,61,2.4f,white,"FABRICACION");
    text_at(RECIPE_X,105,1.23f,muted,"TAB  CAMBIAR SECCION   ENTER  FABRICAR");
    const int first=recipe_scroll(v->recipe_cursor,(int)recipe_count);
    for (size_t i=(size_t)first; i<recipe_count && i<(size_t)(first+RECIPE_ROWS); ++i) {
        const Recipe *r=&recipes[i];
        const float y=RECIPE_Y+(float)((int)i-first)*RECIPE_STEP;
        const bool active=v->crafting && i==(size_t)v->recipe_cursor;
        const bool affordable=g->creative || (g->bag[r->first]>=r->count_first &&
            g->bag[r->second]>=r->count_second && (r->need==AIR || g->bag[r->need]>0));
        char materials[110],label[64];
        if (r->second!=AIR)
            snprintf(materials,sizeof(materials),"%dx %s + %dx %s",r->count_first,
                item_name(r->first),r->count_second,item_name(r->second));
        else snprintf(materials,sizeof(materials),"%dx %s",r->count_first,item_name(r->first));
        if (r->need!=AIR) {
            const size_t used=strlen(materials);
            snprintf(materials+used,sizeof(materials)-used," + [%s]",item_name(r->need));
        }
        snprintf(label,sizeof(label),"%dx %s",r->quantity,item_name(r->output));
        rect(RECIPE_X,y,461,24,active?RGB(56,104,88):(i%2u?RGB(27,48,49):RGB(24,43,44)));
        if (active) rect(RECIPE_X,y,3,24,mint);
        tile_rect(RECIPE_X+7,y+3,18,18,item_icons[r->output],white);
        text_at(RECIPE_X+33,y+8,1.10f,affordable?white:muted,label);
        text_at(RECIPE_X+205,y+8,1.05f,affordable?mint:muted,materials);
    }
    text_at(RECIPE_X,478,1.18f,muted,"E / ESC  VOLVER");
    if (g->notice_time>0) {
        const float w=text_width(1.18f,g->notice);
        text_at(910-w,478,1.18f,gold,g->notice);
    }
}

static void pause_ui(const View *v)
{
    framed_panel(251,94,458,350,panel,RGB(104,158,139));
    rect(251,94,458,3,mint);
    text_at(280,118,3.1f,white,"EN PAUSA");
    static const char *const lines[]={
        "WASD / RATON   MOVERSE Y MIRAR",
        "ESPACIO / SHIFT   SALTAR Y CORRER",
        "CLIC IZQ.   MINAR O ATACAR",
        "CLIC DER.   COLOCAR O COMER",
        "F   COMERCIAR CON UN ALDEANO",
        "RUEDA / 1-9   SELECCIONAR OBJETO"
    };
    for (size_t i=0; i<sizeof(lines)/sizeof(lines[0]); ++i)
        text_at(281,160+(float)i*17,1.28f,muted,lines[i]);
    button(280,273,397,"CONTINUAR",v->menu_cursor==0);
    button(280,316,397,"GUARDAR Y VOLVER AL MENU",v->menu_cursor==1);
    button(280,359,397,"GUARDAR Y SALIR",v->menu_cursor==2);
    text_at(280,414,1.24f,muted,"ENTER  ELEGIR    ESC  CONTINUAR");
}

void draw_frame(Game *g,const View *view,int width,int height)
{
    if (width<=0 || height<=0) return;
    world_view(g,view,width,height);
    if (view->screen==MENU) menu(view);
    if (view->screen==PLAY) hud(g,view);
    if (view->screen==BAG || view->screen==PAUSE) {
        rect(-3000,-3000,6960,6540,RGBA(7,20,24,165));
        if (view->screen==BAG) bag_ui(g,view);
        else pause_ui(view);
    }
    if (view->message && *view->message) {
        const float w=text_width(1.28f,view->message)+20;
        framed_panel(UI_W*.5f-w*.5f,15,w,24,ink,gold);
        text_center(UI_W*.5f,23,1.28f,gold,view->message);
    }
    flush(&dynamic,GL_TRIANGLES);
}

bool draw_capture(const char *path,int width,int height)
{
    if (width<=0 || height<=0) return false;
    const size_t row=(size_t)width*4;
    unsigned char *buf=malloc(row*(size_t)height);
    if (!buf) return false;
    SDL_Surface *image=SDL_CreateRGBSurfaceWithFormat(0,width,height,32,SDL_PIXELFORMAT_RGBA32);
    if (!image) { free(buf); return false; }
    gl.ReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,buf);
    for (int y=0; y<height; ++y)
        memcpy((unsigned char *)image->pixels+(size_t)y*(size_t)image->pitch,
            buf+(size_t)(height-1-y)*row,row);
    bool result=SDL_SaveBMP(image,path)==0;
    SDL_FreeSurface(image);
    free(buf);
    return result;
}
