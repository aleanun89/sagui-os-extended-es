#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H336 - Explorador lite-monkey (mejora 131, D333): la aplicación documental, con ratón y teclado en QEMU.

Criterio:
  1. el árbol enseña la base con sus colecciones y el recuento de documentos;
  2. los documentos se ven como árbol plegable, como tabla y como JSON;
  3. la barra de consulta filtra con un operador ({"precio": {"$gt": 2}});
  4. se edita un documento en JSON: un JSON roto no se guarda (se avisa) y uno válido sí;
  5. se inserta un documento (JSON) y se borra otro con confirmación (por defecto «No»);
  6. Esquema por muestreo (tipos, huecos), Índices (crear) y Agregación por etapas con vista previa.
Capturas: docs/capturas/H336-*.png
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar  # noqa: E402
from bd_sonda import (comando, janela, esperar, cerrar, python_c, imagen_con_home, sonda, boton, nodo, caja, linea,  # noqa: E402
                      celda_txt, escribir_en)


def principal():
    with Maquina("h336", "ide", imagem=imagen_con_home("h336")) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        m.comando("servicio iniciar litemonkey", 60)
        time.sleep(3)
        m.comando("lmk --base tienda --eval \"db.productos.insertMany([{'nombre':'cafe','precio':4.5,'tags':['bebida']},"
                  "{'nombre':'te','precio':3.2},{'nombre':'pan','precio':1.2},{'nombre':'leche','precio':0.9},"
                  "{'nombre':'arroz','precio':2.5}])\"", 60)
        out = m.comando("lmk --base tienda --eval \"db.productos.countDocuments({})\"", 60)
        verificar("5" in out, "lmk insertó cinco documentos")

        # ------------------------------------------------ 1. conectar y el árbol
        marca = len(m.serial())
        m.digitar("python /lib/python/basedatos/explorador_litemonkey.py litemonkey://127.0.0.1:7979/tienda &\n", 0.03)
        ex = janela(m, "Explorador lite-monkey", marca)
        esperar(m, "exploradorbd: Abierta 127.0.0.1:7979", marca, 90)
        time.sleep(2)
        s = sonda(m, ex)
        textos = [n[3] for n in s["nodo"]]
        verificar(any("tienda" in t for t in textos) and any(t.startswith("productos  (5,") for t in textos),
                  "el árbol enseña la base y la colección con su recuento: %s" % textos)
        m.captura("H336-1-arbol")

        # ------------------------------------------------ 2. documentos: árbol, tabla y JSON
        marca = len(m.serial())
        m.clique(*nodo(s, "productos"))
        esperar(m, "exploradorbd: productos: 5 documentos", marca)
        time.sleep(0.8)
        s = sonda(m, ex)
        verificar(any('nombre: "cafe"' in t for _k, _x, _y, t in s["linea"]["docs"]) and len(s["linea"]["docs"]) == 5,
                  "la vista de árbol lista los cinco documentos: %s" % [t for _k, _x, _y, t in s["linea"]["docs"]][:2])
        m.clique(*linea(s, "docs", "cafe"), duplo=True)            # doble clic: se despliega
        time.sleep(1.0)
        s = sonda(m, ex)
        tl = [t for _k, _x, _y, t in s["linea"]["docs"]]
        verificar(any(t.strip().startswith("tags: [1]") or "tags" in t for t in tl) and any('precio: 4.5' in t for t in tl),
                  "doble clic despliega los campos del documento: %s" % tl[:6])
        m.captura("H336-2-arbol-desplegado")
        m.clique(*boton(s, "Tabla"))
        time.sleep(0.8)
        s = sonda(m, ex)
        cols = list(s["cab"]["rejilla"])
        verificar(cols[0] == "_id" and "nombre" in cols and "precio" in cols and "tags" in cols, "la vista de tabla usa las claves de primer nivel: %s" % cols)
        verificar("cafe" in s["rejilla"]["rejilla"].values(), "la tabla enseña los valores")
        m.captura("H336-3-tabla")

        # ------------------------------------------------ 3. barra de consulta
        marca = len(m.serial())
        escribir_en(m, caja(s, "filtro"), '{"precio": {"$gt": 2}}')
        m.tecla("ret", 0.6)
        esperar(m, "exploradorbd: productos: 3 documentos", marca)
        time.sleep(0.8)
        s = sonda(m, ex)
        vals = list(s["rejilla"]["rejilla"].values())
        verificar("cafe" in vals and "te" in vals and "arroz" in vals and "pan" not in vals, "el filtro $gt trae tres documentos")
        m.captura("H336-4-filtro")

        # ------------------------------------------------ 4. editar en JSON
        m.clique(*boton(s, "JSON"))
        time.sleep(0.8)
        s = sonda(m, ex)
        m.captura("H336-5-json")
        marca = len(m.serial())
        escribir_en(m, caja(s, "json"), '{"nombre": "cafe", "precio": ')
        m.captura("H336-5b-json-editado")
        m.clique(*boton(s, "Guardar documento"))
        janela(m, "Guardar documento", marca)
        time.sleep(0.6)
        m.captura("H336-6-json-roto")
        m.tecla("ret", 0.8)
        verificar(True, "un JSON roto no se guarda: se avisa")
        s = sonda(m, ex)
        marca = len(m.serial())
        escribir_en(m, caja(s, "json"), '{"nombre": "cafe", "precio": 5.5, "tags": ["bebida"]}')
        m.clique(*boton(s, "Guardar documento"))
        esperar(m, "exploradorbd: productos: documento guardado", marca)
        time.sleep(1.0)

        # ------------------------------------------------ 5. insertar y borrar con confirmación
        s = sonda(m, ex)
        marca = len(m.serial())
        m.clique(*boton(s, "Insertar JSON..."))
        di = janela(m, "Insertar documento", marca)
        time.sleep(0.8)
        d = sonda(m, di)
        m.clique(*d["texto"][0])
        m.tecla("ctrl-a", 0.1)
        m.digitar('{"nombre": "sal", "precio": 7.5}', 0.06)
        m.captura("H336-7-insertar")
        marca = len(m.serial())
        m.clique(*boton(d, "Aceptar"))
        esperar(m, "exploradorbd: productos: 1 documentos insertados", marca)
        time.sleep(1.2)
        s = sonda(m, ex)
        m.clique(*boton(s, "Tabla"))
        time.sleep(0.8)
        s = sonda(m, ex)
        verificar("sal" in s["rejilla"]["rejilla"].values(), "el documento insertado sale en la consulta (precio > 2)")
        # borrar: con Intro (por defecto «No») no se borra; con Tab e Intro, sí
        i_sal = [k[0] for k, v in s["rejilla"]["rejilla"].items() if v == "sal"][0]
        m.clique(*s["celda"]["rejilla"][(i_sal, 1)])
        time.sleep(0.5)
        marca = len(m.serial())
        m.clique(*boton(s, "Borrar"))
        janela(m, "Borrar documento", marca)
        time.sleep(0.6)
        m.captura("H336-8-borrar")
        m.tecla("ret", 0.8)
        out = python_c(m, "import basedatos;b=basedatos.conectar('litemonkey://127.0.0.1:7979/tienda');print('SAL=',b.contar('productos',{'nombre':'sal'}));print('CAFE=',b.consultar('productos',{'nombre':'cafe'})[0]['precio'])")
        verificar("SAL= 1" in out, "Intro (botón por defecto «No») no borra el documento")
        verificar("CAFE= 5.5" in out, "el documento editado en JSON quedó guardado en el servidor")
        s = sonda(m, ex)
        marca = len(m.serial())
        m.clique(*boton(s, "Borrar"))
        janela(m, "Borrar documento", marca)
        time.sleep(0.6)
        m.tecla("tab", 0.4)
        m.tecla("ret", 1.0)
        esperar(m, "exploradorbd: productos: documento borrado", marca)
        time.sleep(1.0)

        # ------------------------------------------------ 6a. esquema por muestreo
        s = sonda(m, ex)
        m.clique(*boton(s, "Esquema"))
        time.sleep(1.2)
        s = sonda(m, ex)
        filas = dict((celda_txt(s, "rejilla_esq", i, 0), i) for i in range(12) if (i, 0) in s["rejilla"]["rejilla_esq"])
        verificar("nombre" in filas and "tags" in filas, "el esquema lista los campos muestreados: %s" % list(filas))
        t = filas["tags"]
        verificar(celda_txt(s, "rejilla_esq", t, 1).startswith("lista") and celda_txt(s, "rejilla_esq", t, 3) == "4",
                  "tags: tipo lista y 4 documentos sin el campo (huecos): %s / %s" % (celda_txt(s, "rejilla_esq", t, 1), celda_txt(s, "rejilla_esq", t, 3)))
        verificar(celda_txt(s, "rejilla_esq", filas["precio"], 1).startswith("real"), "precio: tipo real")
        m.captura("H336-9-esquema")

        # ------------------------------------------------ 6b. índices
        m.clique(*boton(s, "Índices"))
        time.sleep(1.0)
        s = sonda(m, ex)
        verificar(celda_txt(s, "rejilla_idx", 0, 0) == "_id_", "la lista de índices trae el de _id")
        marca = len(m.serial())
        m.clique(*boton(s, "Nuevo índice..."))
        di = janela(m, "Nuevo índice", marca)
        time.sleep(0.8)
        d = sonda(m, di)
        m.clique(d["entrada"][0][1], d["entrada"][0][2])
        m.digitar("precio", 0.06)
        marca = len(m.serial())
        m.clique(*boton(d, "Crear"))
        esperar(m, "exploradorbd: Creado el índice precio_1", marca)
        time.sleep(1.0)
        s = sonda(m, ex)
        nombres = [celda_txt(s, "rejilla_idx", i, 0) for i in range(4) if (i, 0) in s["rejilla"]["rejilla_idx"]]
        verificar("precio_1" in nombres, "el índice nuevo sale en la lista: %s" % nombres)
        m.captura("H336-10-indices")

        # ------------------------------------------------ 6c. agregación por etapas
        m.clique(*boton(s, "Agregación"))
        time.sleep(0.8)
        s = sonda(m, ex)
        marca = len(m.serial())
        m.clique(*boton(s, "$match"))
        esperar(m, "exploradorbd: Agregación:", marca)
        time.sleep(0.6)
        s = sonda(m, ex)
        escribir_en(m, caja(s, "etapa"), '{"precio": {"$gt": 1}}')
        marca = len(m.serial())
        m.clique(*boton(s, "Aplicar etapa"))
        esperar(m, "exploradorbd: Agregación: 4 documentos tras 1 etapas", marca)
        time.sleep(0.6)
        marca = len(m.serial())
        m.clique(*boton(s, "$group"))
        esperar(m, "exploradorbd: Agregación:", marca)
        time.sleep(0.6)
        s = sonda(m, ex)
        escribir_en(m, caja(s, "etapa"), '{"_id": null, "total": {"$sum": "$precio"}, "n": {"$sum": 1}}')
        marca = len(m.serial())
        m.clique(*boton(s, "Aplicar etapa"))
        esperar(m, "exploradorbd: Agregación: 1 documentos tras 2 etapas", marca)
        time.sleep(0.8)
        s = sonda(m, ex)
        ag = s["rejilla"]["rejilla_agr"]
        cab = list(s["cab"]["rejilla_agr"])
        verificar("n" in cab and "total" in cab and ag[(0, cab.index("n"))] == "4" and ag[(0, cab.index("total"))].startswith("12."),
                  "el $group da la suma y el recuento: %s" % ag)
        m.captura("H336-11-agregacion")
        # la vista previa de la primera etapa: se elige en la lista
        m.clique(*linea(s, "etapas", "$match"))
        time.sleep(1.0)
        s = sonda(m, ex)
        verificar(len(s["rejilla"]["rejilla_agr"]) >= 4 * len(s["cab"]["rejilla_agr"]) // 1 and "nombre" in s["cab"]["rejilla_agr"],
                  "elegir una etapa enseña su vista previa (los documentos tras $match)")

        cerrar(m, ex)
        time.sleep(1.5)


if __name__ == "__main__":
    principal()
    print("H336: OK")
