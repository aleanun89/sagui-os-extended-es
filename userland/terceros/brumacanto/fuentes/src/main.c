#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "ajustes.h"
#include "draw.h"
#include "game.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    SDL_Window *window;
    SDL_GLContext context;
    SDL_AudioDeviceID audio;
    Game *game;
    View view;
    char seed[30], dimensions[3][12], save_path[1024], legacy_path[1024], message[96];
    char *save_dir;
    bool replace_input;
    bool playing, running, left, attack, place, interact;
} App;

static int wrap(int x,int max) { return (x%max+max)%max; }

static bool read_number(const char **next,int *result)
{
    const char *p=*next;
    int value=0;
    if (*p<'0' || *p>'9') return false;
    for (; *p>='0' && *p<='9'; ++p) {
        value=value*10+(*p-'0');
        if (value>MAX_SIDE) return false;
    }
    *next=p;
    *result=value;
    return true;
}

static bool parse_size(const char *arg,WorldSize *size)
{
    if (!read_number(&arg,&size->width)) return false;
    size->depth=size->width;
    if (*arg=='x' || *arg=='X') {
        ++arg;
        if (!read_number(&arg,&size->depth)) return false;
    }
    if (*arg=='x' || *arg=='X') {
        ++arg;
        if (!read_number(&arg,&size->height)) return false;
    }
    return !*arg && valid_world_size(*size);
}

static bool menu_size(const App *a,WorldSize *size)
{
    const char *x=a->dimensions[0],*z=a->dimensions[1],*y=a->dimensions[2];
    return read_number(&x,&size->width) && !*x && read_number(&z,&size->depth) && !*z
        && read_number(&y,&size->height) && !*y && valid_world_size(*size);
}

static void set_size(App *a,WorldSize size)
{
    snprintf(a->dimensions[0],sizeof(a->dimensions[0]),"%u",(unsigned)size.width);
    snprintf(a->dimensions[1],sizeof(a->dimensions[1]),"%u",(unsigned)size.depth);
    snprintf(a->dimensions[2],sizeof(a->dimensions[2]),"%u",(unsigned)size.height);
}

/* Ajustes que se guardan junto a las partidas, en ajustes.txt (D444, D451): la distancia de dibujo (F3) y, en SaguiOS, la escala
 * de dibujo (F4). pref_view es la que se guarda (la de F3, la guardada o la elegida la primera vez); --view manda solo para esa vez. */
static int pref_view=AJUSTE_VISTA_DEFECTO;
static int fps_max=60;   /* D476: tope de cuadros por segundo: 30, 60 o 0 (ilimitado); F6 lo cambia y lo guarda (clave "fps") */
static int probe;   /* SaguiOS: la primera vez, con gpu3d v3, se mide un rato si la vista de 104 va sobrada */
#ifdef SAGUI_BRUMACANTO
/* Escala de dibujo (SaguiOS, D451): 100, 75 o 50 % del tamaño de la ventana; SaguiGL lo amplía al presentar.
 * F4 la cambia y la guarda; --escala vale solo para esa vez. */
static int render_scale=100;
#endif

static bool option_text(const App *a,const char *name,char *data,size_t capacity)
{
    char path[1024];
    if (!a->save_dir || snprintf(path,sizeof(path),"%s%s",a->save_dir,name)>=(int)sizeof(path)) return false;
    SDL_RWops *file=SDL_RWFromFile(path,"rb");
    if (!file) return false;
    const size_t n=SDL_RWread(file,data,1,capacity-1);
    SDL_RWclose(file);
    data[n]=0;
    return true;
}

static void options_save(const App *a)
{
    char path[1024],text[80];
    if (!a->save_dir || snprintf(path,sizeof(path),"%sajustes.txt",a->save_dir)>=(int)sizeof(path)) return;
#ifdef SAGUI_BRUMACANTO
    const int scale=render_scale;
#else
    const int scale=0;
#endif
    const int n=ajuste_formatear_fps(text,sizeof(text),pref_view,scale,fps_max);
    SDL_RWops *file=n>0?SDL_RWFromFile(path,"wb"):NULL;
    if (file) { SDL_RWwrite(file,text,1,(size_t)n); SDL_RWclose(file); }
}

#ifdef SAGUI_BRUMACANTO
static void scale_apply(void)
{
    char text[8];
    snprintf(text,sizeof(text),"%d",render_scale);
    SDL_SetHint(SDL_HINT_SAGUI_GL_ESCALA,text);
}
#endif

static bool settings_path(const App *a,char *path,size_t capacity)
{
    if (!a->save_dir) return false;
    const int n=snprintf(path,capacity,"%sultimo-mundo.txt",a->save_dir);
    return n>=0 && (size_t)n<capacity;
}

static void read_settings(App *a,WorldSize *size)
{
    char path[1024],data[96];
    if (!settings_path(a,path,sizeof(path))) return;
    SDL_RWops *file=SDL_RWFromFile(path,"rb");
    if (!file) return;
    const size_t n=SDL_RWread(file,data,1,sizeof(data)-1);
    SDL_RWclose(file);
    data[n]=0;
    char *separator=strchr(data,'\n');
    if (!separator || separator==data || (size_t)(separator-data)>=sizeof(a->seed)) return;
    WorldSize chosen=default_world_size;
    if (!parse_size(separator+1,&chosen)) return;
    *separator=0;
    memcpy(a->seed,data,(size_t)(separator-data)+1);
    *size=chosen;
}

static void remember_world(const App *a)
{
    char path[1024],data[96];
    if (!settings_path(a,path,sizeof(path))) return;
    const WorldSize size=a->game->world.size;
    const int n=snprintf(data,sizeof(data),"%s\n%dx%dx%d",
        a->seed,size.width,size.depth,size.height);
    if (n<0 || (size_t)n>=sizeof(data)) return;
    SDL_RWops *file=SDL_RWFromFile(path,"wb");
    if (file) {
        SDL_RWwrite(file,data,1,(size_t)n);
        SDL_RWclose(file);
    }
}

static void play_sound(SDL_AudioDeviceID audio,int sound)
{
    static const float pitch[]={0,112,215,168,280,78,480};
    static const float time[]={0,.043f,.102f,.085f,.100f,.19f,.19f};
    if (!audio || sound<SND_STEP || sound>SND_CRAFT || SDL_GetQueuedAudioSize(audio)>7000) return;
    float wave[4300];
    const int n=(int)(time[sound]*22050);
    uint32_t random=(uint32_t)(sound*529);
    for (int i=0; i<n; ++i) {
        const float t=(float)i/22050;
        const float q=(float)i/(float)n;
        const float e=(1-q)*(1-q)*fminf(1,q*22);
        random=random*UINT32_C(1664525)+UINT32_C(1013904223);
        const float noise=(float)(random>>16)/32768-1;
        const float tone=sinf(2*PI*t*pitch[sound]*(sound==SND_CRAFT?1+q*.8f:1-q*.6f));
        const float rough=sound<=SND_PLACE?.6f:.18f;
        wave[i]=(tone*(1-rough)+noise*rough)*e*(sound==SND_STEP?.11f:.20f);
    }
    SDL_QueueAudio(audio,wave,(Uint32)((size_t)n*sizeof(float)));
}

static void open_audio(App *a)
{
    if (SDL_InitSubSystem(SDL_INIT_AUDIO)!=0) return;
    SDL_AudioSpec want={0};
    want.freq=22050;
    want.format=AUDIO_F32SYS;
    want.channels=1;
    want.samples=512;
    a->audio=SDL_OpenAudioDevice(NULL,0,&want,NULL,0);
    if (a->audio) SDL_PauseAudioDevice(a->audio,0);
}

static void screen(App *a,Screen next)
{
    a->view.screen=next;
    a->view.menu_cursor=0;
    a->view.input_field=0;
    a->replace_input=false;
    a->message[0]=0;
    a->left=a->attack=a->place=a->interact=false;
    if (next==PLAY) {
        if (SDL_SetRelativeMouseMode(SDL_TRUE)!=0)
            snprintf(a->message,sizeof(a->message),"Pulsa en la ventana para capturar el raton");
        SDL_StopTextInput();
    } else {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        if (next==MENU) SDL_StartTextInput();
        else SDL_StopTextInput();
    }
}

static void make_paths(App *a,WorldSize size)
{
    a->save_path[0]=a->legacy_path[0]=0;
    if (!a->save_dir || !*a->seed) return;
    const uint32_t seed=seed_hash(a->seed);
    int n=snprintf(a->save_path,sizeof(a->save_path),"%smundo-%08" PRIx32 "-%dx%dx%d.bruma",
        a->save_dir,seed,size.width,size.depth,size.height);
    if (n<0 || (size_t)n>=sizeof(a->save_path)) a->save_path[0]=0;
    n=snprintf(a->legacy_path,sizeof(a->legacy_path),"%smundo-%08" PRIx32 ".bruma",a->save_dir,seed);
    if (n<0 || (size_t)n>=sizeof(a->legacy_path)) a->legacy_path[0]=0;
}

static const char *load_path(const App *a)
{
    if (*a->save_path && save_exists(a->save_path)) return a->save_path;
    if (*a->legacy_path && save_exists(a->legacy_path)) return a->legacy_path;
    return NULL;
}

static void update_options(App *a)
{
    WorldSize size;
    a->view.valid_size=menu_size(a,&size);
    a->view.existing=false;
    if (a->view.valid_size) {
        make_paths(a,size);
        a->view.existing=load_path(a)!=NULL;
    }
    a->message[0]=0;
}

static void random_seed(App *a)
{
    uint32_t r=terrain_hash((uint32_t)SDL_GetPerformanceCounter(),17,86,3);
    snprintf(a->seed,sizeof(a->seed),"%" PRIu32,r);
    update_options(a);
}

static void begin(App *a)
{
    WorldSize size;
    if (!menu_size(a,&size)) {
        snprintf(a->message,sizeof(a->message),"Revisa las dimensiones del mundo");
        return;
    }
    if (!*a->seed) random_seed(a);
    const uint32_t seed=seed_hash(a->seed);
    make_paths(a,size);
    const char *path=load_path(a);
    if (path && !load_game(a->game,path,seed,size)) {
        snprintf(a->message,sizeof(a->message),"No se puede cargar esta partida");
        return;
    }
    if (!path) new_game(a->game,seed,size);
    a->playing=true;
    SDL_Log("BRUMA_MUNDO %s %dx%dx%d semilla %" PRIu32,path?"cargado":"nuevo",size.width,size.depth,size.height,seed);
    screen(a,PLAY);
}

static bool store_game(App *a)
{
    if (!a->playing) return true;
    if (*a->save_path && save_game(a->game,a->save_path)) {
        remember_world(a);
        notify(a->game,"Partida guardada");
        SDL_Log("BRUMA_GUARDADO %s",a->save_path);
        return true;
    }
    snprintf(a->message,sizeof(a->message),"No se pudo guardar la partida");
    return false;
}

static void pause_action(App *a,int index)
{
    if (index==0) { screen(a,PLAY); return; }
    if (!store_game(a)) return;
    if (index==1) {
        a->playing=false;
        screen(a,MENU);
        update_options(a);
    } else a->running=false;
}

static void menu_action(App *a,int index)
{
    if (index==0) begin(a);
    else if (index==1) {
        random_seed(a);
        // Refresh the preview only when committing a generated seed.
        WorldSize size;
        if (menu_size(a,&size)) new_game(a->game,seed_hash(a->seed),size);
        a->view.menu_cursor=0;
    } else a->running=false;
}

static void inventory(App *a)
{
    a->view.item_cursor=a->game->slots[a->game->selected];
    if (a->view.item_cursor==AIR) a->view.item_cursor=GRASS;
    a->view.recipe_cursor=0;
    a->view.crafting=false;
    screen(a,BAG);
}

static void select_item(App *a,int slot)
{
    Game *g=a->game;
    const int id=a->view.item_cursor;
    if (id==BEDROCK || (!g->creative && !g->bag[id])) {
        notify(g,id==BEDROCK?"La roca madre no se coloca":"Todavia no tienes ese objeto");
        return;
    }
    g->slots[slot]=(uint8_t)id;
    g->selected=slot;
    notify(g,"Objeto asignado");
}

static void make_recipe(App *a)
{
    if (craft(a->game,(size_t)a->view.recipe_cursor)) play_sound(a->audio,SND_CRAFT);
    a->game->sound=SND_NONE;
}

static void focus_field(App *a,int field)
{
    a->view.input_field=field;
    a->replace_input=field>0;
    if (field>=0) a->view.menu_cursor=0;
}

static void menu_text(App *a,const char *input)
{
    const int field=a->view.input_field;
    if (field<0) return;
    char *value=field==0 ? a->seed : a->dimensions[field-1];
    const size_t capacity=field==0 ? sizeof(a->seed)-1 : 5;
    size_t length=strlen(value);
    for (const unsigned char *p=(const unsigned char *)input; *p; ++p) {
        if (field==0 ? (*p<32 || *p>=127) : (*p<'0' || *p>'9')) continue;
        if (a->replace_input) { length=0; a->replace_input=false; }
        if (length<capacity) value[length++]=(char)*p;
    }
    value[length]=0;
    a->view.menu_cursor=0;
    update_options(a);
}

static void key_menu(App *a,SDL_Keycode key)
{
    if (key==SDLK_ESCAPE) a->running=false;
    else if (key==SDLK_UP || key==SDLK_DOWN) {
        focus_field(a,-1);
        a->view.menu_cursor=wrap(a->view.menu_cursor+(key==SDLK_UP?-1:1),3);
    } else if (key==SDLK_TAB) {
        const int direction=SDL_GetModState()&KMOD_SHIFT ? -1 : 1;
        focus_field(a,wrap(a->view.input_field+1+direction,5)-1);
    } else if (key==SDLK_a && (SDL_GetModState()&(KMOD_CTRL|KMOD_GUI)) && a->view.input_field>=0) {
        a->replace_input=true;
    }
    else if (key==SDLK_RETURN || key==SDLK_KP_ENTER) menu_action(a,a->view.menu_cursor);
    else if (key==SDLK_BACKSPACE && a->view.input_field>=0) {
        const int field=a->view.input_field;
        char *value=field==0 ? a->seed : a->dimensions[field-1];
        if (*value) value[a->replace_input?0:strlen(value)-1]=0;
        a->replace_input=false;
        update_options(a);
        a->view.menu_cursor=0;
    }
}

static void key_bag(App *a,SDL_Keycode key)
{
    View *v=&a->view;
    if (key==SDLK_ESCAPE || key==SDLK_e) screen(a,PLAY);
    else if (key==SDLK_TAB) v->crafting=!v->crafting;
    else if (key>=SDLK_1 && key<=SDLK_9) select_item(a,(int)(key-SDLK_1));
    else if (v->crafting) {
        if (key==SDLK_UP) v->recipe_cursor=wrap(v->recipe_cursor-1,(int)recipe_count);
        if (key==SDLK_DOWN) v->recipe_cursor=wrap(v->recipe_cursor+1,(int)recipe_count);
        if (key==SDLK_RETURN || key==SDLK_KP_ENTER) make_recipe(a);
    } else {
        int amount=0;
        if (key==SDLK_UP) amount=-BAG_COLUMNS;
        if (key==SDLK_DOWN) amount=BAG_COLUMNS;
        if (key==SDLK_RIGHT) amount=1;
        if (key==SDLK_LEFT) amount=-1;
        v->item_cursor=wrap(v->item_cursor-1+amount,ITEM_COUNT-1)+1;
        if (key==SDLK_RETURN || key==SDLK_KP_ENTER) select_item(a,a->game->selected);
    }
}

static void key_event(App *a,SDL_Keycode key)
{
    Game *g=a->game;
    switch (a->view.screen) {
    case MENU: key_menu(a,key); break;
    case BAG: key_bag(a,key); break;
    case PAUSE:
        if (key==SDLK_ESCAPE) screen(a,PLAY);
        else if (key==SDLK_UP) a->view.menu_cursor=wrap(a->view.menu_cursor-1,3);
        else if (key==SDLK_DOWN || key==SDLK_TAB) a->view.menu_cursor=wrap(a->view.menu_cursor+1,3);
        else if (key==SDLK_RETURN || key==SDLK_KP_ENTER) pause_action(a,a->view.menu_cursor);
        break;
    case PLAY:
        if (key==SDLK_ESCAPE) screen(a,PAUSE);
        else if (key==SDLK_e) inventory(a);
        else if (key==SDLK_f) a->interact=true;
        else if (key==SDLK_F5) store_game(a);
        else if (key==SDLK_F3) {
            /* distancia de dibujo (SaguiOS, D444): 32, 48, 64, 80 y 104 bloques */
            static const int steps[]={32,48,64,80,VIEW_DISTANCE_MAX};
            int next=steps[0];
            for (int i=0; i<5; ++i) if (steps[i]>draw_get_view_distance()) { next=steps[i]; break; }
            draw_set_view_distance(next);
            pref_view=draw_get_view_distance();
            options_save(a);   /* la elegida se guarda y vale la proxima vez */
            probe=0;
            SDL_Log("BRUMA_VISTA %d (F3, guardada)",next);
            char text[64];
            snprintf(text,sizeof(text),"Distancia de vision: %d",next);
            notify(g,text);
        }
#ifdef SAGUI_BRUMACANTO
        else if (key==SDLK_F4) {
            render_scale=render_scale==100?75:render_scale==75?50:100;
            scale_apply();
            options_save(a);
            char text[64];
            snprintf(text,sizeof(text),"Escala de dibujo: %d %%",render_scale);
            notify(g,text);
        }
#endif
        else if (key==SDLK_F6) {
            /* limitador de FPS (D476): 60, 30 o ilimitado */
            fps_max=ajuste_fps_siguiente(fps_max);
            options_save(a);
            SDL_Log("BRUMA_FPS %d (F6, guardado)",fps_max);
            char text[64];
            if (fps_max) snprintf(text,sizeof(text),"Limite de FPS: %d",fps_max);
            else snprintf(text,sizeof(text),"Limite de FPS: ilimitado");
            notify(g,text);
        }
        else if (key==SDLK_F2) {
            g->creative=!g->creative;
            notify(g,g->creative?"Modo creativo":"Modo supervivencia");
        } else if (key>=SDLK_1 && key<=SDLK_9) g->selected=(int)(key-SDLK_1);
        break;
    }
}

static void cursor(App *a,int x,int y,bool clicked)
{
    int w,h;
    float u,v;
    SDL_GetWindowSize(a->window,&w,&h);
    ui_pointer(w,h,x,y,&u,&v);
    if (a->view.screen==MENU || a->view.screen==PAUSE) {
        const bool main=a->view.screen==MENU;
        if (main && clicked && u>=MENU_X && u<MENU_X+MENU_BUTTON_W) {
            if (v>=SEED_Y && v<SEED_Y+FIELD_HEIGHT) { focus_field(a,0); return; }
            if (v>=SIZE_Y && v<SIZE_Y+FIELD_HEIGHT) {
                const int relative=(int)(u-MENU_X),field=relative/SIZE_STEP;
                if (field<3 && relative%SIZE_STEP<SIZE_WIDTH) focus_field(a,field+1);
                return;
            }
        }
        int bx=main?MENU_X:280, by=main?MENU_Y:273, width=main?MENU_BUTTON_W:397;
        if (u<bx || u>=bx+width || v<by || v>=by+3*MENU_STEP) return;
        const int offset=(int)(v-by);
        if (offset%MENU_STEP>=35) return;
        const int index=offset/MENU_STEP;
        a->view.menu_cursor=index;
        if (clicked) {
            if (main) { focus_field(a,-1); menu_action(a,index); }
            else pause_action(a,index);
        }
    } else if (a->view.screen==BAG && clicked) {
        if (u>=BAG_X && v>=BAG_Y && u<BAG_X+BAG_COLUMNS*BAG_STEP) {
            const int row=(int)(v-BAG_Y),column=(int)(u-BAG_X);
            const int id=1+(row/BAG_STEP)*BAG_COLUMNS+column/BAG_STEP;
            if (id<ITEM_COUNT && row%BAG_STEP<41 && column%BAG_STEP<42) {
                a->view.item_cursor=id;
                a->view.crafting=false;
            }
        } else if (u>=RECIPE_X && u<911 && v>=RECIPE_Y) {
            const int row=(int)(v-RECIPE_Y);
            const int index=recipe_scroll(a->view.recipe_cursor,(int)recipe_count)+row/RECIPE_STEP;
            if ((size_t)index<recipe_count && row/RECIPE_STEP<RECIPE_ROWS && row%RECIPE_STEP<24) {
                if (a->view.crafting && a->view.recipe_cursor==index) make_recipe(a);
                a->view.crafting=true;
                a->view.recipe_cursor=index;
            }
        }
    }
}

static void events(App *a)
{
    SDL_Event ev;
    Game *g=a->game;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_QUIT:
            if (store_game(a)) a->running=false;
            else if (a->view.screen==PLAY) {
                screen(a,PAUSE);
                snprintf(a->message,sizeof(a->message),"No se pudo guardar la partida");
            }
            break;
        case SDL_WINDOWEVENT:
            if (ev.window.event==SDL_WINDOWEVENT_FOCUS_LOST && a->view.screen==PLAY) screen(a,PAUSE);
            break;
        case SDL_KEYDOWN:
            if (!ev.key.repeat || (a->view.screen==MENU && ev.key.keysym.sym==SDLK_BACKSPACE))
                key_event(a,ev.key.keysym.sym);
            break;
        case SDL_TEXTINPUT:
            if (a->view.screen==MENU) menu_text(a,ev.text.text);
            break;
        case SDL_MOUSEMOTION:
            if (a->view.screen==PLAY) {
                g->yaw=fmodf(g->yaw+(float)ev.motion.xrel*.0026f,2*PI);
                g->pitch=clampf(g->pitch-(float)ev.motion.yrel*.0026f,-1.55f,1.55f);
            } else cursor(a,ev.motion.x,ev.motion.y,false);
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (a->view.screen==PLAY) {
                if (!SDL_GetRelativeMouseMode() && SDL_SetRelativeMouseMode(SDL_TRUE)==0)
                    a->message[0]=0;
                if (ev.button.button==SDL_BUTTON_LEFT) a->left=a->attack=true;
                if (ev.button.button==SDL_BUTTON_RIGHT) a->place=true;
            } else if (ev.button.button==SDL_BUTTON_LEFT) cursor(a,ev.button.x,ev.button.y,true);
            break;
        case SDL_MOUSEBUTTONUP:
            if (ev.button.button==SDL_BUTTON_LEFT) a->left=false;
            break;
        case SDL_MOUSEWHEEL:
            if (a->view.screen==PLAY) {
                const int direction=ev.wheel.direction==SDL_MOUSEWHEEL_FLIPPED?-1:1;
                g->selected=wrap(g->selected-direction*ev.wheel.y,SLOTS);
            }
            break;
        default: break;
        }
        if (!a->running) return;
    }
}

static bool graphics(App *a,bool hidden)
{
    SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER,"1");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    const Uint32 flags=SDL_WINDOW_OPENGL|SDL_WINDOW_ALLOW_HIGHDPI|SDL_WINDOW_RESIZABLE|
        (hidden?SDL_WINDOW_HIDDEN:SDL_WINDOW_SHOWN);
    a->window=SDL_CreateWindow("Brumacanto",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
        1152,648,flags);
    if (!a->window) return false;
    SDL_SetWindowMinimumSize(a->window,640,360);
    a->context=SDL_GL_CreateContext(a->window);
    if (!a->context) return false;
    SDL_GL_SetSwapInterval(1);
    char assets[1024];
#ifdef SAGUI_BRUMACANTO
    /* SaguiOS: los gráficos van en /apps/brumacanto (no junto al ejecutable, que está en /apps/bin). */
    const int n=snprintf(assets,sizeof(assets),"%s","/apps/brumacanto/");
#else
    char *base=SDL_GetBasePath();
    const int n=snprintf(assets,sizeof(assets),"%sassets/",base?base:"");
    SDL_free(base);
#endif
    return n>=0 && (size_t)n<sizeof(assets) && draw_init(assets);
}

static bool smoke(App *a,const char *prefix)
{
    int w,h;
    SDL_GL_GetDrawableSize(a->window,&w,&h);
    if (w<=0 || h<=0) return false;
    static const Screen states[]={MENU,PLAY,BAG,PAUSE};
    static const char *const names[]={"menu","mundo","mochila","pausa"};
    for (int i=0; i<4; ++i) {
        a->view.screen=states[i];
        if (i==1) for (int s=0; s<120; ++s) {
            Input in={.forward=s<40 ? 1.f:0};
            tick(a->game,in,1.f/120.f);
        }
        draw_frame(a->game,&a->view,w,h);
        if (draw_error()!=0) { SDL_Log("Error de render en %s",names[i]); return false; }
        if (prefix) {
            char file[1024];
            int n=snprintf(file,sizeof(file),"%s-%s.bmp",prefix,names[i]);
            if (n<0 || (size_t)n>=sizeof(file) || !draw_capture(file,w,h)) return false;
        }
        SDL_GL_SwapWindow(a->window);
    }
    Game *g=a->game;
    const int x=g->world.min_x+17,z=g->world.max_z-21;
    g->pos=(Vec3){x+.5f,surface_at(&g->world,x,z)+1.02f,z+.5f};
    refresh_wildlife(g,true);
    aim(g);
    a->view.screen=PLAY;
    draw_frame(g,&a->view,w,h);
    if (draw_error()!=0 || g->world.failed) return false;
    SDL_GL_SwapWindow(a->window);
    SDL_Log("Prueba OpenGL ES 1.1 OK: menu, mundo, mochila, pausa y terreno lejano (%d x %d x %d)",
        g->world.size.width,g->world.size.depth,g->world.size.height);
    return true;
}

int main(int argc,char **argv)
{
    const char *seed_arg=NULL,*capture=NULL;
    int bench_frames=0,scale_arg=0,view_arg=0;
    bool force_auto=false;
    WorldSize size=default_world_size;
    bool check=false,explicit_size=false,fullscreen=false;
    for (int i=1; i<argc; ++i) {
        if (strcmp(argv[i],"--seed")==0 && i+1<argc) seed_arg=argv[++i];
        else if (strcmp(argv[i],"--size")==0 && i+1<argc) {
            explicit_size=true;
            size=default_world_size;
            if (!parse_size(argv[++i],&size)) {
                fprintf(stderr,"Tamano invalido. X/Z: %d-%d en multiplos de %d. Y: %d-%d.\n",
                    MIN_SIDE,MAX_SIDE,2*CHUNK,MIN_HEIGHT,MAX_HEIGHT);
                return 1;
            }
        }
        else if (strcmp(argv[i],"--view")==0 && i+1<argc) {   /* solo para esta vez; "auto" vuelve a elegirla segun el equipo */
            ++i;
            if (strcmp(argv[i],"auto")==0) force_auto=true;
            else view_arg=atoi(argv[i]);
        }
        else if (strcmp(argv[i],"--bench")==0 && i+1<argc) bench_frames=atoi(argv[++i]);
        else if (strcmp(argv[i],"--smoke-test")==0) check=true;
        else if (strcmp(argv[i],"--fullscreen")==0) fullscreen=true;   /* SaguiOS, D451 (F11 la cambia luego) */
        else if (strcmp(argv[i],"--escala")==0 && i+1<argc) {   /* SaguiOS, D451 (F4 la cambia luego) */
            scale_arg=atoi(argv[++i]);
            if (scale_arg!=50 && scale_arg!=75 && scale_arg!=100) { fprintf(stderr,"Escala: 50, 75 o 100.\n"); return 1; }
        }
        else if (strcmp(argv[i],"--capture-prefix")==0 && i+1<argc) capture=argv[++i];
        else {
            printf("Uso: brumacanto [--seed SEMILLA] [--size X[xZ[xY]]] [--view 32-104|auto] [--bench CUADROS] [--fullscreen] [--escala 50|75|100] [--smoke-test [--capture-prefix RUTA]]\n");
            return strcmp(argv[i],"--help")!=0;
        }
    }
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO)!=0) {
        fprintf(stderr,"SDL2: %s\n",SDL_GetError());
        return 1;
    }
    App a={0};
    a.running=true;
    a.game=calloc(1,sizeof(*a.game));
    a.save_dir=check?NULL:SDL_GetPrefPath("Brumacanto","Brumacanto");
    a.view.seed_text=a.seed;
    for (int i=0; i<3; ++i) a.view.dimensions[i]=a.dimensions[i];
    a.view.message=a.message;
    a.view.item_cursor=PICK;
    int status=1;
    if (!a.game || !graphics(&a,check)) {
        fprintf(stderr,"Error al iniciar SDL2/OpenGL ES 1.1: %s\n",SDL_GetError());
#ifdef _WIN32
        if (!check) {
            char startup[440];
            snprintf(startup,sizeof(startup),
                "No se pudo iniciar SDL2/OpenGL ES 1.1.\n%s\n\nConsulta README.md.",
                SDL_GetError());
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Brumacanto",startup,a.window);
        }
#endif
        goto done;
    }
    if (fullscreen && !check) SDL_SetWindowFullscreen(a.window,SDL_WINDOW_FULLSCREEN_DESKTOP);
    /* Ajustes (D444, D451): escala y distancia de dibujo. La distancia: --view manda; si no, la guardada (F3); la primera vez, la que
     * toca segun el equipo (SaguiOS: sin 3D por la GPU o con una GPU sin gpu3d v3, 48; con v3, 64 y, si una medida corta va sobrada,
     * 104) y en otros sistemas 64. */
    const char *view_origin="por defecto";
    if (!check) {
        char text[96]={0};
        const bool have=option_text(&a,"ajustes.txt",text,sizeof(text));
        int saved=have && !force_auto ? ajuste_leer(text,"vista") : -1;
        fps_max=ajuste_fps_valido(have ? ajuste_leer(text,"fps") : -1);   /* D476 */
        if (getenv("BRUMA_FPS")) fps_max=ajuste_fps_valido(atoi(getenv("BRUMA_FPS")));   /* solo para esa vez (pruebas) */
        SDL_Log("BRUMA_FPS %d",fps_max);
#ifdef SAGUI_BRUMACANTO
        int scale_saved=have ? ajuste_leer(text,"escala") : -1;
        if (!have && option_text(&a,"escala.txt",text,sizeof(text))) scale_saved=atoi(text);   /* el fichero de antes (D451) */
        if (scale_saved==50 || scale_saved==75 || scale_saved==100) render_scale=scale_saved;
        if (scale_arg) render_scale=scale_arg;
        scale_apply();   /* --escala vale para esta vez; F4 la guarda */
#endif
        if (saved>=AJUSTE_VISTA_MIN && saved<=AJUSTE_VISTA_MAX) {
            pref_view=saved;
            view_origin="guardada";
        } else {
#ifdef SAGUI_BRUMACANTO
            const char *renderer=draw_renderer();
            pref_view=ajuste_vista_para(renderer);
            const bool v3=strstr(renderer,"GPU v3")!=NULL;
            view_origin=v3?"automatica: GPU con v3":strstr(renderer,"(GPU)")?"automatica: GPU sin v3":"automatica: sin 3D por la GPU";
            probe=v3 && view_arg<=0;
#endif
            options_save(&a);
        }
        draw_set_view_distance(pref_view);
    }
    if (view_arg>0) draw_set_view_distance(view_arg);
    SDL_Log("BRUMA_VISTA_AJUSTE %d (%s)",pref_view,view_origin);
    SDL_Log("BRUMA_VISTA %d (%s)",draw_get_view_distance(),view_arg>0?"--view":view_origin);
    snprintf(a.seed,sizeof(a.seed),"Bruma-160");
    if (seed_arg && strlen(seed_arg)<sizeof(a.seed)) strcpy(a.seed,seed_arg);
    else if (seed_arg) {
        fprintf(stderr,"La semilla puede tener hasta %zu bytes.\n",sizeof(a.seed)-1);
        goto done;
    }
    if (!seed_arg && !explicit_size) read_settings(&a,&size);
    set_size(&a,size);
    update_options(&a);
    new_game(a.game,seed_hash(a.seed),size);
    if (check) { status=smoke(&a,capture)?0:1; goto done; }

    open_audio(&a);
    screen(&a,MENU);
    update_options(&a);
    if (seed_arg) begin(&a);
    const Uint64 clockrate=SDL_GetPerformanceFrequency();
    Uint64 previous=SDL_GetPerformanceCounter();
    const float step=1.f/120.f;
    float carry=0;
    Uint64 bench_start=0,bench_worst=0;
    int bench_done=0,probe_frames=0;
    double probe_ms=0;
#define PROBE_SKIP 45
#define PROBE_FRAMES 45
    while (a.running) {
        const Uint64 start=SDL_GetPerformanceCounter();
        const float dt=fminf(.10f,(float)((double)(start-previous)/(double)clockrate));
        if (bench_frames>0 && a.view.screen==PLAY) {
            /* Medición de cuadros por segundo (SaguiOS, D444): camina hacia delante girando despacio. */
            if (bench_done==0) bench_start=start;
            else if (start-previous>bench_worst) bench_worst=start-previous;
            a.game->yaw+=dt*.6f;
            if (++bench_done>bench_frames) {
                const double s=(double)(start-bench_start)/(double)clockrate;
                SDL_Log("BRUMA_BENCH cuadros %d tiempo_ms %d fps_x10 %d peor_cuadro_ms %d vista %d\n",bench_frames,
                    (int)(s*1000),(int)(bench_frames*10/s),(int)((double)bench_worst*1000/(double)clockrate),
                    draw_get_view_distance());
                a.running=false;
                break;
            }
        }
        previous=start;
        events(&a);
        if (!a.running) break;
        if (a.game->world.failed && a.view.screen==PLAY) {
            screen(&a,PAUSE);
            snprintf(a.message,sizeof(a.message),"No hay memoria para cargar mas terreno");
        }
        a.view.animation+=dt;
        if (a.view.screen==PLAY) {
            carry+=dt;
            const Uint8 *k=SDL_GetKeyboardState(NULL);
            while (carry>=step) {
                const Input in={
                    .forward=bench_frames>0?1.f:(float)k[SDL_SCANCODE_W]-(float)k[SDL_SCANCODE_S],
                    .side=(float)k[SDL_SCANCODE_D]-(float)k[SDL_SCANCODE_A],
                    .jump=k[SDL_SCANCODE_SPACE]!=0,
                    .sprint=k[SDL_SCANCODE_LSHIFT] || k[SDL_SCANCODE_RSHIFT],
                    .mine=a.left || a.attack,.attack=a.attack,.place=a.place,.interact=a.interact};
                tick(a.game,in,step);
                if (a.game->sound) { play_sound(a.audio,a.game->sound); a.game->sound=SND_NONE; }
                a.attack=a.place=a.interact=false;
                carry-=step;
            }
        } else {
            carry=0;
            if (a.view.screen==BAG) a.game->notice_time=fmaxf(0,a.game->notice_time-dt);
        }
        int w,h;
        SDL_GL_GetDrawableSize(a.window,&w,&h);
        draw_frame(a.game,&a.view,w,h);
        SDL_GL_SwapWindow(a.window);
        const float used=(float)((double)(SDL_GetPerformanceCounter()-start)/(double)clockrate);
        if (probe && a.view.screen==PLAY && bench_frames<=0) {
            /* SaguiOS, primera vez con gpu3d v3: unos cuadros con la vista de 64 (sin los primeros, que generan terreno); si el trabajo
             * por cuadro es corto, la distancia de 104 va sobrada y se pasa a ella (y se guarda). */
            if (++probe_frames>PROBE_SKIP) probe_ms+=(double)used*1000.0;
            if (probe_frames>=PROBE_SKIP+PROBE_FRAMES) {
                const double ms=probe_ms/PROBE_FRAMES;
                const int next=ajuste_vista_medida(ms);
                SDL_Log("BRUMA_VISTA medida %d decimas de ms por cuadro con la vista %d -> %d",(int)(ms*10),draw_get_view_distance(),next);
                probe=0;
                if (next!=pref_view && draw_get_view_distance()==pref_view) {
                    pref_view=next;
                    draw_set_view_distance(next);
                    options_save(&a);
                    notify(a.game,"Distancia de vision: 104 (el equipo va sobrado)");
                }
            }
        }
        /* D476: tope de cuadros (60 = 15 ms de antes; 30; o ilimitado); la espera duerme */
        if (fps_max>0 && bench_frames<=0) {
            /* plazo acumulado: SDL_Delay redondea al tic del sistema y asi el ritmo medio sale exacto */
            static Uint32 base; static unsigned nc; static int fps_ant;
            const Uint32 ahora=SDL_GetTicks();
            if (fps_ant!=fps_max || (int)(ahora-(base+nc*1000u/(unsigned)fps_max))>100) { fps_ant=fps_max; base=ahora; nc=0; }
            else {
                const int falta=(int)(base+(++nc)*1000u/(unsigned)fps_max-ahora);
                if (falta>0) SDL_Delay((Uint32)falta);
            }
        }
    }
    status=0;
done:
    if (a.audio) SDL_CloseAudioDevice(a.audio);
    if (a.context) { draw_close(); SDL_GL_DeleteContext(a.context); }
    if (a.window) SDL_DestroyWindow(a.window);
    SDL_free(a.save_dir);
    if (a.game) close_world(&a.game->world);
    free(a.game);
    SDL_Quit();
    return status;
}
