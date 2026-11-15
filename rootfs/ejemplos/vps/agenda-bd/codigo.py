# Agenda de contactos: una rejilla y una ficha sobre la misma tabla de SQLite (mejora 127, D304).
# La base se crea con unos contactos la primera vez; vive en /home (o en /tmp si no hay /home).
import os
import basedatos

RUTA = "/home/agenda.db" if os.path.isdir("/home") else "/tmp/agenda.db"


def preparar():
    bd = basedatos.conectar("sqlite:" + RUTA)
    if "contactos" not in bd.tablas():
        bd.ejecutar("CREATE TABLE contactos (id INTEGER PRIMARY KEY, nombre TEXT NOT NULL, "
                    "telefono TEXT, ciudad TEXT, favorito INTEGER DEFAULT 0)")
        for n, t, c, fav in (("Ana Perez", "600 111 222", "Madrid", 1), ("Beto Lopez", "611 333 444", "Sevilla", 0),
                             ("Carla Diaz", "622 555 666", "Lima", 0), ("Dani Ruiz", "633 777 888", "Bogota", 1)):
            bd.insertar("contactos", {"nombre": n, "telefono": t, "ciudad": c, "favorito": fav})
    bd.cerrar()


def Formulario_cargar():
    preparar()
    ConectorBD1.origen = "sqlite:" + RUTA
    ConectorBD1.abrir()


def ConectorBD1_error():
    Etiqueta4.texto = "Error: " + ConectorBD1.error
