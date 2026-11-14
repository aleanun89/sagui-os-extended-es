#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H88 - lite-monkey, la base de datos documental (mejora 067, D117).

Imagen de consola con /t/ (los casos del cliente y el script de rendimiento,
los mismos de litemonkey/testes/) y un disco de datos en /home. Criterio:
  1. servidor en SaguiOS y cliente desde el PC (CPython, hostfwd 7979):
     testes/casos_cliente.py sin fallos;
  2. cliente y servidor dentro de SaguiOS (loopback 127.0.0.1): el mismo
     casos_cliente.py con MicroPython, sin fallos; la consola lmk con --eval;
  3. varios clientes a la vez: 4 hilos del PC y un MicroPython de SaguiOS;
  4. servidor en el PC y cliente MicroPython en SaguiOS (10.0.2.2);
  5. persistencia: shutdown, reinicio de la VM con el mismo disco y los datos
     siguen (y el índice);
  6. rendimiento en SaguiOS (MicroPython por loopback y CPython desde el PC):
     se imprime, sin umbral.
"""
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, RAIZ, SAIDA, FalhaTeste  # noqa: E402

LMK = os.path.join(RAIZ, "litemonkey")
sys.path.insert(0, os.path.join(LMK, "python"))
import litemonkey  # noqa: E402


def puerto_libre():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def construir_imagen():
    b = os.path.join(RAIZ, "build")
    raiz = os.path.join(SAIDA, "h88-rootfs")
    shutil.rmtree(raiz, ignore_errors=True)
    shutil.copytree(os.path.join(b, "rootfs"), raiz)
    os.makedirs(os.path.join(raiz, "t"))
    for f in ("casos_cliente.py", "rendimiento.py"):
        shutil.copy(os.path.join(LMK, "testes", f), os.path.join(raiz, "t", f))
    with open(os.path.join(raiz, "t", "hilo.py"), "w") as f:
        f.write("import sys, litemonkey\n"
                "c = litemonkey.Cliente(sys.argv[1], int(sys.argv[2]))\n"
                "col = c.varios.datos\n"
                "for i in range(100):\n"
                "    col.insert_one({'quien': 'sagui', 'i': i})\n"
                "print('hilo sagui:', col.count_documents({'quien': 'sagui'}))\n")
    sgfs = os.path.join(SAIDA, "h88.sgfs")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mksgfs.py"), raiz, sgfs], check=True, stdout=subprocess.DEVNULL)
    img = os.path.join(SAIDA, "h88.img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
                    "--ramdisk", sgfs, "--saida", img, "--vesa", "--console", "--dados-mb", "32"],
                   check=True, stdout=subprocess.DEVNULL)
    return img


def esperar_puerto(puerto, tempo=60):
    fim = time.time() + tempo
    while time.time() < fim:
        try:
            c = litemonkey.Cliente("127.0.0.1", puerto, timeout=5)
            c.close()
            return True
        except litemonkey.errors.ConnectionFailure:
            time.sleep(0.5)
    return False


def casos_desde_pc(puerto, base):
    env = dict(os.environ, PYTHONPATH=os.path.join(LMK, "python"))
    r = subprocess.run([sys.executable, os.path.join(LMK, "testes", "casos_cliente.py"), "127.0.0.1", str(puerto), base],
                       capture_output=True, text=True, env=env, timeout=600)
    return r.stdout + r.stderr


def ejecutar(m, texto, tempo):
    """m.comando() y, además, espera a que el programa termine de verdad.

    comando() da por acabado el programa en cuanto cuenta un «sagui:» más en la
    serie, y con el servidor en segundo plano (-v) eso no basta: cuando una
    conexión del PC se abre o se cierra mientras se teclea, el servidor lo anota
    y el sagsh redibuja la línea con el prompt (TECLA_B_REDESENHAR). comando()
    volvía entonces antes de ejecutar nada; con los tiempos de la v7.2
    pasaba casi siempre en la parte 3 (total 400 o 464 en vez de 500). Aquí se
    espera al aviso del kernel de que el proceso terminó."""
    ini = len(m.serial())
    m.comando(texto, tempo=tempo)
    fim = time.time() + tempo
    while "terminou com codigo" not in m.serial()[ini:]:
        if time.time() > fim:
            raise FalhaTeste(f"{texto!r} no terminó en {tempo} s")
        time.sleep(0.2)
    return m.serial()[ini:].replace("\r", "")


def main():
    subprocess.run(["make", "-s", "-C", LMK, "build/litemonkey"], check=True)   # el servidor del PC (parte 4)
    img = construir_imagen()
    fwd = puerto_libre()
    extras = ["-nic", f"user,model=e1000,hostfwd=tcp:127.0.0.1:{fwd}-:7979"]

    # Servidor del PC para la parte 4
    tmp = tempfile.mkdtemp(prefix="h88-")
    pc_puerto = puerto_libre()
    servidor_pc = subprocess.Popen([os.path.join(LMK, "build", "litemonkey"), "--puerto", str(pc_puerto), "--datos", tmp,
                                    "--escuchar", "127.0.0.1"], stderr=subprocess.DEVNULL)
    try:
        with Maquina("h88", "ide", imagem=img, extras=extras, snapshot=False) as m:
            m.esperar_serial("sagui:/$", 120)
            m.comando("litemonkey -v &", tempo=30)
            m.esperar_serial("escuchando en", 60)
            verificar("datos en /home/litemonkey" in m.serial(), "litemonkey arranca con los datos en /home/litemonkey")

            # 1. del PC a SaguiOS
            verificar(esperar_puerto(fwd), "el PC llega al servidor de SaguiOS (hostfwd 7979)")
            out = casos_desde_pc(fwd, "desde_pc")
            res = [l for l in out.splitlines() if l.startswith("FALLO") or l.startswith("casos:") or "Error" in l]
            verificar("casos:" in out and ", 0 fallos" in out, "servidor en SaguiOS, cliente CPython en el PC: %s" % res[-3:])

            # 2. loopback: cliente MicroPython y servidor en el mismo SaguiOS
            out = ejecutar(m, "python /t/casos_cliente.py 127.0.0.1 7979 loopback", tempo=900)
            res = [l for l in out.splitlines() if l.startswith("FALLO") or l.startswith("casos:") or "Error" in l]
            verificar("casos:" in out and ", 0 fallos" in out, "cliente MicroPython por 127.0.0.1 (loopback): %s" % res[-3:])
            out = ejecutar(m, 'lmk 127.0.0.1 --base loopback --eval "db.gente.count_documents({})" --eval "db.list_collection_names()"',
                            tempo=120)
            verificar('[ "gente" ]' in out, "consola lmk con --eval")

            # 3. varios clientes a la vez
            res = {}

            def hilo(k):
                try:
                    c = litemonkey.Cliente("127.0.0.1", fwd, timeout=120)
                    col = c.varios.datos
                    for i in range(100):
                        col.insert_one({"quien": k, "i": i})
                    res[k] = col.count_documents({"quien": k})
                    c.close()
                except Exception as e:  # noqa: BLE001
                    res[k] = repr(e)
            hilos = [threading.Thread(target=hilo, args=(k,)) for k in range(4)]
            for h in hilos:
                h.start()
            out = ejecutar(m, "python /t/hilo.py 127.0.0.1 7979", tempo=300)
            for h in hilos:
                h.join(300)
            c = litemonkey.Cliente("127.0.0.1", fwd, timeout=60)
            total = c.varios.datos.count_documents({})
            verificar(all(res.get(k) == 100 for k in range(4)) and "hilo sagui: 100" in out and total == 500,
                      "4 clientes del PC y 1 de SaguiOS a la vez: %r, total %d" % (res, total))

            # 6. rendimiento (antes de parar el servidor)
            out = ejecutar(m, "python /t/rendimiento.py 127.0.0.1 7979 3000", tempo=900)
            rend_sagui = [l for l in out.splitlines() if l.startswith("rendimiento:")]
            verificar(len(rend_sagui) >= 8, "rendimiento con MicroPython por loopback (%d medidas)" % len(rend_sagui))
            env = dict(os.environ, PYTHONPATH=os.path.join(LMK, "python"))
            r = subprocess.run([sys.executable, os.path.join(LMK, "testes", "rendimiento.py"), "127.0.0.1", str(fwd), "5000"],
                               capture_output=True, text=True, env=env, timeout=900)
            rend_pc = [l for l in r.stdout.splitlines() if l.startswith("rendimiento:")]
            verificar(len(rend_pc) >= 8, "rendimiento con CPython desde el PC contra SaguiOS (%d medidas)" % len(rend_pc))
            print("  --- servidor en SaguiOS, cliente MicroPython en SaguiOS (3000 docs):")
            for l in rend_sagui:
                print("   ", l)
            print("  --- servidor en SaguiOS, cliente CPython en el PC (5000 docs):")
            for l in rend_pc:
                print("   ", l)

            # 4. servidor en el PC, cliente MicroPython en SaguiOS
            out = ejecutar(m, f"python /t/casos_cliente.py 10.0.2.2 {pc_puerto} desde_sagui", tempo=900)
            res = [l for l in out.splitlines() if l.startswith("FALLO") or l.startswith("casos:") or "Error" in l]
            verificar("casos:" in out and ", 0 fallos" in out, "servidor en el PC, cliente MicroPython en SaguiOS: %s" % res[-3:])

            # 5. persistencia: índice, shutdown y reinicio
            c.varios.datos.create_index("i")
            c.shutdown()
            m.esperar_serial("litemonkey: cerrado", 60)
            m.captura("H88-litemonkey", guardar=False)

        with Maquina("h88-b", "ide", imagem=img, extras=extras, snapshot=False) as m:
            m.esperar_serial("sagui:/$", 120)
            m.comando("litemonkey &", tempo=30)
            m.esperar_serial("escuchando en", 120)
            verificar(esperar_puerto(fwd), "el servidor vuelve a arrancar tras reiniciar la VM")
            c = litemonkey.Cliente("127.0.0.1", fwd, timeout=60)
            col = c.varios.datos
            n = col.count_documents({})
            plan = col.find({"i": 5}).explain()["queryPlanner"]["winningPlan"]["stage"]
            verificar(n == 500 and plan == "IXSCAN", "persistencia en /home: %d documentos e índice (%s)" % (n, plan))
            verificar(c.loopback.gente.count_documents({}) > 200 and "desde_pc" in c.list_database_names(),
                      "las bases de las partes 1 y 2 siguen ahí")
            c.shutdown()
            m.esperar_serial("litemonkey: cerrado", 60)
    finally:
        servidor_pc.terminate()
        servidor_pc.wait()
        shutil.rmtree(tmp, ignore_errors=True)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print("FALHOU:", e)
        sys.exit(1)
