# SPDX-License-Identifier: GPL-2.0-or-later
# errors.py - excepciones del cliente lite-monkey (los nombres de pymongo).


class LiteMonkeyError(Exception):
    """Base de todos los errores del cliente."""


PyMongoError = LiteMonkeyError


class ConnectionFailure(LiteMonkeyError):
    """No se pudo conectar o la conexión se cortó."""


AutoReconnect = ConnectionFailure
ServerSelectionTimeoutError = ConnectionFailure


class InvalidId(LiteMonkeyError):
    """Texto que no es un ObjectId (24 cifras hexadecimales)."""


class InvalidName(LiteMonkeyError):
    """Nombre de base o colección no válido."""


class InvalidDocument(LiteMonkeyError):
    """Un valor que no se puede codificar en BSON."""


class OperationFailure(LiteMonkeyError):
    """El servidor respondió con un error ({"ok": 0, "errmsg", "code"})."""

    def __init__(self, error, code=None, details=None):
        super().__init__(error)
        self.code = code
        self.details = details or {}

    def __str__(self):
        msg = self.args[0] if self.args else ""
        return "%s, código: %s" % (msg, self.code) if self.code is not None else str(msg)


class WriteError(OperationFailure):
    """Un error de escritura (inserción, actualización o borrado)."""


class DuplicateKeyError(WriteError):
    """Clave repetida en el _id o en un índice único (código 11000)."""


class BulkWriteError(OperationFailure):
    """Errores en insert_many: details["writeErrors"] y details["nInserted"]."""


class CursorNotFound(OperationFailure):
    """El cursor caducó (10 minutos sin usarlo) o ya se cerró."""


class AuthenticationFailed(OperationFailure):
    """Usuario o clave incorrectos."""


def desde_respuesta(r):
    """La excepción adecuada para una respuesta con ok = 0."""
    codigo = r.get("code")
    msg = r.get("errmsg", "error del servidor")
    if codigo == 11000:
        return DuplicateKeyError(msg, codigo, r)
    if codigo == 43:
        return CursorNotFound(msg, codigo, r)
    if codigo == 18:
        return AuthenticationFailed(msg, codigo, r)
    return OperationFailure(msg, codigo, r)
