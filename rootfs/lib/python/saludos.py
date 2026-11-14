# SPDX-License-Identifier: GPL-2.0-or-later
# saludos.py - módulo de ejemplo en /lib/python (D067).
# Cualquier programa puede usarlo con "import saludos": /lib/python está
# siempre en sys.path.

def hola(nombre="mundo"):
    return "¡Hola, %s! Saludos desde /lib/python." % nombre


def despedida(nombre="mundo"):
    return "Hasta luego, %s." % nombre
