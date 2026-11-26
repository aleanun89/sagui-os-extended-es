# prueba_asyncio.py - asyncio de MicroPython en SaguiOS (D478): gather y create_task con esperas en milisegundos.
import asyncio


async def f(n, d):
    await asyncio.sleep_ms(d)
    return n * 2


async def main():
    r = await asyncio.gather(f(1, 300), f(2, 100), f(3, 200))
    print("gather", r)
    t = asyncio.create_task(f(5, 50))
    print("tarea", await t)

asyncio.run(main())
