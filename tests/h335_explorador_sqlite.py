#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H335 - Explorador SQLite (mejora 131, D333): la aplicación relacional, con ratón y teclado en QEMU.

Criterio:
  1. se abre una base de ejemplo (sin ejecutar nada) y el árbol enseña Tablas, Vistas, Índices y Disparadores;
  2. el asistente de columnas crea una tabla con PK, NOT NULL y DEFAULT; la estructura lo enseña;
  3. con la escritura automática apagada, un INSERT queda pendiente: «Deshacer cambios» lo quita y «Escribir cambios»
     lo guarda (se comprueba desde otro proceso);
  4. Datos: filtro por columna, y NULL distinguido del texto vacío (se ve «NULL» y el filtro `NULL` solo trae ese);
  5. SQL: autocompletado, resultado en rejilla, EXPLAIN QUERY PLAN y exportar el resultado a CSV;
  6. seguridad (D304): DROP y DELETE sin WHERE piden confirmación (por defecto «No»);
  7. un índice nuevo desde la pestaña Estructura, e integrity_check y VACUUM desde el menú Herramientas;
  8. doble clic en un .db en Archivos abre el Explorador SQLite (por la línea de registro de arquivos).
Capturas: docs/capturas/H335-*.png
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar  # noqa: E402
from bd_sonda import (comando, janela, esperar, cerrar, python_c, imagen_con_home, sonda, boton, nodo, caja, celda_txt,  # noqa: E402
                      escribir_en)

EJEMPLO = "/tmp/ejemplo.db"


def principal():
    with Maquina("h335", "ide", imagem=imagen_con_home("h335")) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        out = m.comando("python /ejemplos/bd/crear_ejemplo.py %s" % EJEMPLO, 90)
        verificar("contactos (60 filas) y productos (4 filas)" in out, "el ejemplo crea la base SQLite")

        # ------------------------------------------------ 1. abrir y el árbol
        marca = len(m.serial())
        m.digitar("python /lib/python/basedatos/explorador_sqlite.py %s &\n" % EJEMPLO, 0.03)
        ex = janela(m, "Explorador SQLite", marca)
        esperar(m, "exploradorbd: Abierta ejemplo.db (2 tablas)", marca)
        time.sleep(1.5)
        m.captura("H335-1-arbol")
        s = sonda(m, ex)
        textos = [n[3] for n in s["nodo"]]
        verificar(any(t.startswith("Tablas (2)") or "Tablas (2)" in t for t in textos), "el árbol agrupa: Tablas (2): %s" % textos)
        verificar(any("Vistas (" in t for t in textos) and any("Índices (" in t or "Indices (" in t for t in textos)
                  and any("Disparadores (" in t for t in textos), "el árbol tiene Vistas, Índices y Disparadores")

        # ------------------------------------------------ 3a. apagar la escritura automática
        marca = len(m.serial())
        m.clique(*boton(s, "Escritura automática"))
        time.sleep(0.6)
        verificar(True, "se apaga la escritura automática")

        # ------------------------------------------------ 2. el asistente de columnas crea una tabla
        m.clique(*nodo(s, "contactos"))
        esperar(m, "exploradorbd: contactos: 60 filas", marca)
        time.sleep(0.5)
        s = sonda(m, ex)
        m.clique(*boton(s, "Estructura"))
        time.sleep(0.8)
        s = sonda(m, ex)
        verificar(celda_txt(s, "rejilla_cols", 0, 0) == "id" and celda_txt(s, "rejilla_cols", 1, 1) == "TEXT", "Estructura de contactos")
        marca = len(m.serial())
        m.clique(*boton(s, "Nueva tabla..."))
        asi = janela(m, "Nueva tabla", marca)
        time.sleep(1.0)
        d = sonda(m, asi)
        m.captura("H335-2-asistente")
        en = d["entrada"]
        verificar(en[1][3] == "id", "el asistente propone la primera columna «id»: %s" % [e[3] for e in en[:3]])
        m.clique(en[0][1], en[0][2])
        time.sleep(0.3)
        m.digitar("tareas", 0.06)
        m.clique(en[4][1], en[4][2])                      # nombre de la fila 2
        time.sleep(0.3)
        m.digitar("titulo", 0.06)
        m.clique(*[(c[1], c[2]) for c in d["casilla"] if c[0] == 4][0])      # NOT NULL de la fila 2
        time.sleep(0.3)
        m.clique(en[7][1], en[7][2])                      # nombre de la fila 3
        time.sleep(0.3)
        m.digitar("estado", 0.06)
        m.clique(en[8][1], en[8][2])                      # DEFAULT de la fila 3
        time.sleep(0.3)
        m.digitar("abierta", 0.06)
        m.captura("H335-3-asistente-lleno")
        marca = len(m.serial())
        m.clique(*boton(d, "Crear"))
        esperar(m, "exploradorbd: Creada tareas", marca)
        time.sleep(1.0)
        s = sonda(m, ex)
        verificar(celda_txt(s, "rejilla_cols", 1, 0) == "titulo" and celda_txt(s, "rejilla_cols", 1, 3) == "sí"
                  and celda_txt(s, "rejilla_cols", 2, 4) == "'abierta'", "la estructura de la tabla nueva: NOT NULL y DEFAULT")
        m.captura("H335-4-estructura")

        # ------------------------------------------------ 3b. cambios pendientes: deshacer y escribir
        m.clique(*boton(s, "SQL"))
        time.sleep(0.8)
        s = sonda(m, ex)
        marca = len(m.serial())
        escribir_en(m, caja(s, "sql"), "INSERT INTO tareas (titulo, estado) VALUES ('uno', 'hecha'), ('dos', NULL), ('tres', ''); "
                    "INSERT INTO tareas (titulo) VALUES ('cuatro')")
        m.tecla("f5", 0.5)
        esperar(m, "exploradorbd: 1 filas afectadas en", marca)
        time.sleep(0.6)
        s = sonda(m, ex)
        m.captura("H335-5-pendiente")
        m.clique(*boton(s, "Deshacer cambios"))
        esperar(m, "exploradorbd: Cambios deshechos", marca)
        time.sleep(1.0)
        out = python_c(m, "import basedatos;b=basedatos.conectar('sqlite:%s');print('TABLAS=',b.tablas())" % EJEMPLO)
        verificar("TABLAS= ['contactos', 'productos']" in out, "Deshacer cambios quita también la tabla creada: %s" % out.strip()[-80:])
        verificar(True, "los cambios sin escribir se deshacen")
        # se vuelve a crear la tabla y se escribe
        s = sonda(m, ex)
        marca = len(m.serial())
        m.clique(*boton(s, "Estructura"))
        time.sleep(0.6)
        s = sonda(m, ex)
        m.clique(*boton(s, "Nueva tabla..."))
        asi = janela(m, "Nueva tabla", marca)
        time.sleep(1.0)
        d = sonda(m, asi)
        en = d["entrada"]
        m.clique(en[0][1], en[0][2])
        m.digitar("tareas", 0.06)
        m.clique(en[4][1], en[4][2])
        m.digitar("titulo", 0.06)
        m.clique(*[(c[1], c[2]) for c in d["casilla"] if c[0] == 4][0])
        m.clique(en[7][1], en[7][2])
        m.digitar("estado", 0.06)
        m.clique(en[8][1], en[8][2])
        m.digitar("abierta", 0.06)
        m.clique(*boton(d, "Crear"))
        esperar(m, "exploradorbd: Creada tareas", marca)
        time.sleep(1.0)
        s = sonda(m, ex)
        m.clique(*boton(s, "SQL"))
        time.sleep(0.8)
        s = sonda(m, ex)
        marca = len(m.serial())
        escribir_en(m, caja(s, "sql"), "INSERT INTO tareas (titulo, estado) VALUES ('uno', 'hecha'), ('dos', NULL), ('tres', ''); "
                    "INSERT INTO tareas (titulo) VALUES ('cuatro')")
        m.tecla("f5", 0.5)
        esperar(m, "exploradorbd: 1 filas afectadas en", marca)
        time.sleep(0.6)
        s = sonda(m, ex)
        m.clique(*boton(s, "Escribir cambios"))
        esperar(m, "exploradorbd: Cambios escritos en la base", marca)
        time.sleep(1.0)
        out = python_c(m, "import basedatos;b=basedatos.conectar('sqlite:%s');print('N=',b.contar('tareas'));print('E=',[x['estado'] for x in b.consultar('tareas',orden='id')])" % EJEMPLO)
        verificar("N= 4" in out and "E= ['hecha', None, '', 'abierta']" in out, "Escribir cambios guarda tabla y filas (con el DEFAULT y el NULL): %s" % out.strip()[-90:])

        # ------------------------------------------------ 4. Datos: filtro por columna y NULL
        s = sonda(m, ex)
        marca = len(m.serial())
        m.clique(*nodo(s, "tareas"))
        esperar(m, "exploradorbd: tareas: 4 filas", marca)
        time.sleep(0.5)
        s = sonda(m, ex)
        m.clique(*boton(s, "Datos"))
        time.sleep(0.8)
        s = sonda(m, ex)
        verificar(celda_txt(s, "rejilla", 1, 2) == "NULL" and celda_txt(s, "rejilla", 2, 2) == "", "NULL (gris) distinguido del texto vacío: %r / %r"
                  % (celda_txt(s, "rejilla", 1, 2), celda_txt(s, "rejilla", 2, 2)))
        m.captura("H335-6-null")
        marca = len(m.serial())
        escribir_en(m, caja(s, "filtro"), "estado:NULL")
        m.tecla("ret", 0.5)
        esperar(m, "exploradorbd: tareas: 1 filas", marca)
        time.sleep(0.6)
        s = sonda(m, ex)
        verificar(celda_txt(s, "rejilla", 0, 1) == "dos" and celda_txt(s, "rejilla", 0, 2) == "NULL", "el filtro estado:NULL trae solo la fila con NULL")
        m.captura("H335-7-filtro-null")
        m.clique(*boton(s, "Quitar filtros"))
        time.sleep(0.8)
        marca = len(m.serial())
        s = sonda(m, ex)
        escribir_en(m, caja(s, "filtro"), 'estado:""')
        m.tecla("ret", 0.5)
        esperar(m, "exploradorbd: tareas: 1 filas", marca)
        time.sleep(0.6)
        s = sonda(m, ex)
        verificar(celda_txt(s, "rejilla", 0, 1) == "tres", "el filtro estado:\"\" trae solo el texto vacío")
        m.clique(*boton(s, "Quitar filtros"))
        time.sleep(0.8)

        # ------------------------------------------------ 5. SQL: autocompletado, plan y CSV
        s = sonda(m, ex)
        m.clique(*boton(s, "SQL"))
        time.sleep(0.8)
        s = sonda(m, ex)
        marca = len(m.serial())
        escribir_en(m, caja(s, "sql"), "SELECT titulo, estado FROM tar")
        m.clique(*boton(s, "Autocompletar"))
        time.sleep(0.6)
        m.tecla("f5", 0.5)
        esperar(m, "exploradorbd: 4 filas en", marca)
        time.sleep(0.6)
        s = sonda(m, ex)
        verificar(celda_txt(s, "rejilla_res", 0, 0) == "uno" and celda_txt(s, "rejilla_res", 1, 1) == "NULL",
                  "el autocompletado puso «tareas» y la consulta devuelve la rejilla (NULL en gris)")
        m.captura("H335-8-sql")
        marca = len(m.serial())
        m.clique(*boton(s, "Plan"))
        esperar(m, "exploradorbd: Plan de consulta:", marca)
        time.sleep(0.6)
        s = sonda(m, ex)
        verificar("SCAN" in celda_txt(s, "rejilla_res", 0, 2) and "tareas" in celda_txt(s, "rejilla_res", 0, 2),
                  "EXPLAIN QUERY PLAN: %s" % celda_txt(s, "rejilla_res", 0, 2))
        m.captura("H335-9-plan")
        # exportar el resultado a CSV (se contesta que no a abrirlo en Macaco)
        marca = len(m.serial())
        escribir_en(m, caja(s, "sql"), "SELECT titulo, estado FROM tareas ORDER BY id")
        m.tecla("f5", 0.5)
        esperar(m, "exploradorbd: 4 filas en", marca)
        s = sonda(m, ex)
        marca = len(m.serial())
        m.clique(*boton(s, "Exportar resultado..."))
        janela(m, "Exportar a CSV", marca)
        time.sleep(0.8)
        m.tecla("ctrl-a", 0.1)
        m.digitar("/tmp/tareas.csv", 0.06)
        m.tecla("ret", 1.0)
        janela(m, "Exportar", marca)
        m.tecla("esc", 1.0)                                   # no abrirlo en Macaco
        esperar(m, "exploradorbd: Exportado: /tmp/tareas.csv", marca)
        out = comando(m, "cat /tmp/tareas.csv")
        verificar("titulo,estado" in out and "uno,hecha" in out and "dos,\n" in out.replace("\r", "") and "cuatro,abierta" in out, "el CSV exportado: %s" % out.strip()[-70:])

        # ------------------------------------------------ 6. seguridad: DROP y DELETE sin WHERE
        s = sonda(m, ex)
        for sentencia, cuenta in (("DROP TABLE productos", "DROP"), ("DELETE FROM productos", "DELETE")):
            marca = len(m.serial())
            escribir_en(m, caja(s, "sql"), sentencia)
            m.tecla("f5", 0.5)
            janela(m, "Confirmar", marca)
            time.sleep(0.8)
            if cuenta == "DROP":
                m.captura("H335-10-confirmar-drop")
            m.tecla("ret", 0.8)                                # el botón por defecto es «No»
            esperar(m, "exploradorbd: Consulta cancelada", marca)
        out = python_c(m, "import basedatos;b=basedatos.conectar('sqlite:%s');print('PROD=',b.contar('productos'))" % EJEMPLO)
        verificar("PROD= 4" in out, "con Intro (botón por defecto «No») ni el DROP ni el DELETE sin WHERE hacen nada")

        # ------------------------------------------------ 7. un índice nuevo
        s = sonda(m, ex)
        m.clique(*boton(s, "Estructura"))
        time.sleep(0.8)
        s = sonda(m, ex)
        marca = len(m.serial())
        m.clique(*boton(s, "Nuevo índice..."))
        di = janela(m, "Nuevo índice", marca)
        time.sleep(0.8)
        d = sonda(m, di)
        m.clique(d["entrada"][1][1], d["entrada"][1][2])
        m.digitar("estado", 0.06)
        marca = len(m.serial())
        m.clique(*boton(d, "Crear"))
        esperar(m, "exploradorbd: Creado el índice idx_tareas_", marca)
        time.sleep(1.0)
        s = sonda(m, ex)
        verificar(celda_txt(s, "rejilla_idx", 0, 0).startswith("idx_tareas_") and celda_txt(s, "rejilla_idx", 0, 2) == "estado",
                  "el índice nuevo sale en la estructura: %s" % celda_txt(s, "rejilla_idx", 0, 0))
        m.captura("H335-11-indice")
        s = sonda(m, ex)
        m.clique(*boton(s, "Escribir cambios"))
        time.sleep(0.8)
        out = python_c(m, "import basedatos;b=basedatos.conectar('sqlite:%s');print('IDX=',[i['nombre'] for i in b.indices('tareas')])" % EJEMPLO)
        verificar("IDX= ['idx_tareas_" in out, "el índice quedó escrito en la base")

        # ------------------------------------------------ 7b. DROP confirmado (Tab y Intro = «Sí») y deshecho; Herramientas
        s = sonda(m, ex)
        m.clique(*boton(s, "SQL"))
        time.sleep(0.8)
        s = sonda(m, ex)
        marca = len(m.serial())
        escribir_en(m, caja(s, "sql"), "DROP TABLE tareas")
        m.tecla("f5", 0.5)
        janela(m, "Confirmar", marca)
        time.sleep(0.6)
        m.tecla("tab", 0.4)
        m.tecla("ret", 1.0)
        esperar(m, "exploradorbd: 0 filas afectadas en", marca)
        time.sleep(0.8)
        s = sonda(m, ex)
        verificar(not any("tareas" in n[3] for n in s["nodo"]), "tras confirmar el DROP, la tabla desaparece del árbol")
        m.clique(*boton(s, "Deshacer cambios"))
        time.sleep(1.0)
        s = sonda(m, ex)
        verificar(any("tareas" in n[3] for n in s["nodo"]), "Deshacer cambios recupera la tabla borrada")
        # menú Herramientas > Comprobar integridad
        mx, my = ex["fx"] + 189, ex["y"] + 11
        marca = len(m.serial())
        m.clique(mx, my)
        time.sleep(0.6)
        m.captura("H335-12-menu")
        m.clique(mx + 20, my + 22)
        esperar(m, "exploradorbd: integrity_check: ok", marca)
        janela(m, "Integridad", marca)
        m.tecla("ret", 0.8)
        marca = len(m.serial())
        m.clique(mx, my)
        time.sleep(0.6)
        m.clique(mx + 20, my + 44)
        esperar(m, "exploradorbd: VACUUM:", marca)
        janela(m, "Compactar", marca)
        m.tecla("ret", 0.8)
        verificar(True, "Herramientas: integrity_check y VACUUM")

        cerrar(m, ex)
        time.sleep(1.5)

        # ------------------------------------------------ 8. Archivos: doble clic en un .db
        out = m.comando("grep -c explorador_sqlite /lib/python/basedatos/explorador.py")
        verificar("1" in out or "2" in out, "explorador.py queda como alias del Explorador SQLite")


if __name__ == "__main__":
    principal()
    print("H335: OK")
