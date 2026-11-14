# SPDX-License-Identifier: GPL-2.0-or-later
# litemonkey - cliente Python de lite-monkey, la base de datos documental de
# SaguiOS (puerto 7979). Funciona igual en CPython y en MicroPython.
from .bson import ObjectId, SON, Regex, Fecha, Binary, Timestamp, encode, decode
from .cliente import (Cliente, MongoClient, Base, Coleccion, Cursor, CommandCursor, InsertOneResult,
                      InsertManyResult, UpdateResult, DeleteResult, PUERTO)
from . import errors

ASCENDING = 1
DESCENDING = -1
version = "1"
__version__ = version
