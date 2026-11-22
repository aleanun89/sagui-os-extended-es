<!-- categoria: Conoce SaguiOS/Sistemas operativos; orden: 13; titulo: Appendix, the SDL library in SaguiOS -->
# Appendix, the SDL library in SaguiOS

[Español](anexo-sdl.md) · [Português](anexo-sdl_PT.md) · [English](anexo-sdl_EN.md)

> What SDL is, why SaguiOS has its own, how it fits the system and what had to be done, with the topics of this course, to port games and programs.

## 1. Theory

### 1.1 What SDL is

SDL (*Simple DirectMedia Layer*) is a library that gives a program windows, drawing, keyboard, mouse, audio, threads and timers through a single
interface, whatever the system. SDL 1.2 is the classic version; SDL 2 is the current one, with a renderer, textures and richer events. On Linux it
relies on X11 or Wayland; on Windows, on its own APIs.

### 1.2 Why a layer is needed

A program that uses SDL only knows the interface; the implementation changes from one system to another. That is why SaguiOS does not depend on X11
or Wayland (it has neither): it implements the SDL interface on top of its own graphics library, libsagui. The game is compiled without changes.

### 1.3 Window, canvas, event and frame

- **Window**: what the system shows.
- **Canvas**: a 32-bit memory area where drawing happens; it is copied to the window when presented.
- **Events**: a queue filled by the keyboard, the mouse and the window; the program empties it with `SDL_PollEvent`.
- **Frame**: one cycle of events, drawing, presentation and waiting. A game repeats the frame dozens of times per second.

### 1.4 The course topics, applied

- **Threads**: creating a thread and waiting for it to end; what each thread may do.
- **Locks and conditions**: a mutex protects the data and a condition signals a change; the wait must be repeated in a loop.
- **Audio**: sound is mixed in a callback that runs on a thread of its own; if that thread does not arrive in time, the sound breaks up.
- **Priorities**: an audio thread can ask the scheduler for more CPU.
- **Time**: the system clock and waits, without timers with callbacks.

## 2. How it fits in SaguiOS

### 2.1 What is there

- **Window and canvas**: a logical window of the size the program asked for; the canvas is scaled to the screen size, with black bands if needed, and at
  320 × 200 and 640 × 400 the VGA 4:3 proportion is kept (`userland/libsdl2/sdl2_sagui.c`, decision D204).
- **Keyboard and mouse**: SDL 2 key codes come from the kernel's key map, so there are KEYDOWN and KEYUP events also for Shift, Ctrl and Alt.
- **Audio**: by callback or by queue (`SDL_QueueAudio`); the conversion of format, channels and frequency is in `SDL_BuildAudioCVT` and
  `SDL_ConvertAudio` (decision D383). `SDL_PauseAudio(0)` starts an own **audio thread** that calls the callback with the device lock held
  (decision D393).
- **Software and GPU renderer**: with `SDL_CreateRenderer(..., SDL_RENDERER_ACCELERATED)` the canvas goes to the GPU if there is one with 3D; if not, it
  stays in software without the program noticing (decision D288, `docs/sdl-gpu.md`). `SDL_RenderGeometry` draws triangles (D436 uses it in AoEN).
- **Full screen without going through RAM**: the GPU writes the frame to the screen (decision D432).
- **OpenGL ES 1.1**: `userland/libsdl2/sdl2_gl.c` implements `SDL_GL_*` over EGL and SaguiGL (decision D425).

### 2.2 Two libraries: SDL 1.2 and SDL 2

- `userland/libsdl12/` is the common SDL 1.2 (`sdl_sagui.c`, decision D228).
- `userland/libsdl2/` is SDL 2 (`sdl2_sagui.c`, decision D230), with its headers, `SDL_mixer`, `SDL_image` and `SDL_ttf` next to it.
- Examples of ports that use these libraries: Command & Conquer (D230 and D247), DOOM with Chocolate Doom (D383), Freeciv (D374), mGBA (D322),
  Brumacanto (D444) and AoEN (D393, D431, D436).

**What is missing.** There is no `SDL_AddTimer` (it returns an error) nor timers with callbacks, nor game controllers, nor relative mouse, and custom cursors
are accepted but not drawn. The header comment of `userland/libsdl2/SDL.h` used to say there were no threads or OpenGL; it is no longer true (D393 added
threads and `sdl2_gl.c` implements OpenGL ES) and the header is up to date.

## 3. What had to be done, and why

### 3.1 Threads: `SDL_CreateThread` over `thread_create`

Until decision D383, `SDL_CreateThread` failed and the SDL locks did nothing. Decision D393 connected them to the system's real threads (`thread_create`
of libsagui, decision D167). Two rules remain:

- A worker thread **must not call** video, events or audio: it only computes and signals the main thread.
- A program that used to call itself while holding a lock keeps working, because the same thread may take it again.

### 3.2 Locks and conditions

`SDL_CreateMutex`, `SDL_LockMutex` and `SDL_CondSignal` use the system's `mutex_t` and futex (decision D150). A condition may wake up without anyone signalling
it, so the wait always runs in a loop that checks the data again (the `hilo_avisa.c` example does this).

A `SDL_CondWait` on the main thread is cut every 2 ms (to advance OpenAL, which is moved from the main thread) and then it checks again (D393). Whoever waits
for the audio callback, like Chocolate Doom, gets the signal for real from the audio thread.

### 3.3 Audio: a thread of its own so the sound does not break

At first the SaguiOS SDL moved the audio **from the main thread**, once per frame, with about 120 ms of lead: if a frame took longer (Brumacanto has 400 ms
frames when it builds new chunks), the sound broke. Decision D393 (extension of 2026-11-22) gave the SDL an **audio thread**: `SDL_PauseAudio(0)` (the first time)
creates it with `thread_create`, and every 5 ms it looks at how much is left to play in the system stream; if it is less than the kernel latency plus 60 ms (at
least 120 ms), it calls the program's callback **from that thread** and hands it the block. Since the thread does not depend on the game's frames, a slow frame no
longer breaks the sound. `SDL_SAGUI_AUDIO_HILO=0` (environment variable or hint) goes back to the old mode. AoEN also has its own mixer without SDL (D431).

Practical consequence: `SDL_LockAudio` and `SDL_LockAudioDevice` are now real (recursive) locks, and the callback runs with it held. If the program touches from
another thread what it shares with the callback (a volume, a counter, the position of a piece of music), it must take it: they are two threads and without it there
would be a race condition (chapter 7). It is the pattern of `tono.c`.

### 3.4 Priorities: `PLAN_AUDIO`

The scheduler's audio priority is `PLAN_AUDIO`, decision **D436** (D431 is something else: AoEN audio independent of the FPS). A thread asks for it with the
system call; the SDL audio thread **only asks for it when told to** (`SDL_SAGUI_AUDIO_PRIO=1`, variable or hint), because with it Freeciv, with client, server and
music on the same CPU, stopped answering the server's pings. It is limited by usage (`PLAN_AUDIO_MAX_USO`, 5 ticks per about 250 ms, around 20 % of a CPU), so it cannot
be used to monopolize the CPU. It is the topic of chapter 5 in practice.

### 3.5 Timers and the clock

Programs use `SDL_GetTicks` and `SDL_Delay`. `SDL_AddTimer` is not available (it returns an error), so a program that depends on a timer with a callback
must write its own loop comparing the time, as the `tono.c` example does.

**Decisions and limits.**
- **Decision: an own SDL over libsagui.** It lets programs be ported without depending on X11 or Wayland (D230).
- **Decision: optional GPU renderer.** If the GPU fails, the program stays in software (D288).
- **Decision: an audio thread of its own in the SDL.** Decision D393 (extension) keeps a slow frame from breaking the sound, at the cost of the callback running
  on another thread and having to be protected with `SDL_LockAudio`.
- **Limit: the audio priority is not turned on by default.** If the machine is heavily loaded, the audio thread competes with the others like any other; `PLAN_AUDIO` can
  be requested with `SDL_SAGUI_AUDIO_PRIO=1`.
- **Limit: there are no timers with callbacks.** Loops with the clock must be used instead.

## 4. Examples

Three small programs are in `/ejemplos/sdl/` (in the repository, `rootfs/ejemplos/sdl/`). According to the TCC guide (`tcc.md`) they compile inside
SaguiOS from a folder of your own like this:

```
cd /tmp
tcc /ejemplos/sdl/ventana_mueve.c -o ventana_mueve -lSDL2
tcc /ejemplos/sdl/hilo_avisa.c -o hilo_avisa -lSDL2
tcc /ejemplos/sdl/tono.c -o tono -lSDL2
```

And they run with `./ventana_mueve`, `./hilo_avisa` and `./tono`.

- `ventana_mueve.c`: a 320 × 200 window with a rectangle that moves with the arrow keys (events and renderer). Escape quits.
- `hilo_avisa.c`: a thread computes a sum and signals the main thread with a condition (threads, mutex and conditions).
- `tono.c`: a 440 Hz tone generated in the audio callback; at the end it prints how many times it was called and whether another thread did it (audio and clock).

**Status of the checks.** The three have been compiled and run inside SaguiOS under QEMU (`tests/curso_ejemplos.py`, with and without KVM).
`hilo_avisa` prints `suma de 1 a 1000000 = 500000500000`. `tono` says the callback ran about 85 times in two seconds and on a thread other than the main
one (the test image has no sound card, so nothing is heard: the audio thread keeps the pace with the clock). In `ventana_mueve`, six right arrows and
three down arrows move the rectangle 24 and 12 points, and Escape closes the window. The test led to two fixes: `hilo_avisa.c` summed with a `long`,
which on SaguiOS (32 bits) has 32 bits and overflowed (it printed 1784293664 instead of 500000500000; a reminder that on 32 bits `long` and pointers
fit in 32 bits, chapter 6), and the title "Rectángulo" came out as "RectAingulo" because TCC leaves strings in UTF-8 and the window draws them as Latin-1
(the title now has no accent).

## 5. Review questions

1. Why does SaguiOS implement its own SDL instead of using X11 or Wayland? What advantage does it give to a game already written?
2. In `hilo_avisa.c`, why is the wait a `while` and not an `if`? What would happen if the worker thread called `SDL_PollEvent`?
3. A game has 150 ms frames and the sound does not break up with the default SDL. Why? What would happen with `SDL_SAGUI_AUDIO_HILO=0`?
4. What is `PLAN_AUDIO`, who asks for it and why does it have a usage limit?
5. Why does the audio callback now need `SDL_LockAudio` to touch data shared with the program?

## 6. Answers

1. X11 and Wayland do not exist in SaguiOS, so the own layer is necessary. For a game already written against SDL, the advantage is that it compiles
   without changes: the interface is the same and only the implementation changes.
2. Because a condition may wake up without a signal, and the data must be checked again after every wake-up. And the worker thread must not call
   `SDL_PollEvent`, because events and video are handled only on the main thread.
3. Because since decision D393 the audio is moved by a thread of the SDL's own, which feeds the card with about 120 ms of lead without depending on the game's
   frames. With `SDL_SAGUI_AUDIO_HILO=0` it would go back to the old mode, in which the main thread moves the audio once per frame: with 150 ms frames, the sound would
   break up. (AoEN also has its own mixer: D431.)
4. `PLAN_AUDIO` is the audio priority of the scheduler (D436). A thread asks for it with the system call. It has a usage limit so that a thread cannot
   monopolize the CPU by always asking for that priority.
5. Because the callback runs on the audio thread, different from the program's thread: without the lock, both could touch the same data at once (race
   condition, chapter 7).
