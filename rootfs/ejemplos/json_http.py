# SPDX-License-Identifier: GPL-2.0-or-later
# json_http.py - pedir datos JSON por HTTP o HTTPS con requests.
# Ejecuta con: python /ejemplos/json_http.py [dirección]
# Sin dirección pregunta a la API pública de GitHub (necesita internet).
# requests vive en /lib/python/requests.py; el HTTPS usa la libtls de SaguiOS
# y los certificados raíz de /etc/ssl/certs (ver el comando "certificados").
import sys
import requests

url = sys.argv[1] if len(sys.argv) > 1 else "https://api.github.com/repos/micropython/micropython"
print("Pidiendo", url, "...")
try:
    r = requests.get(url, headers={"Accept": "application/json"}, timeout=15)
except requests.RequestException as e:
    print("No se pudo:", e)
    sys.exit(1)

print("Estado:", r.status_code, r.reason)
print("Tipo:", r.headers.get("content-type"))
try:
    datos = r.json()
except ValueError:
    print("La respuesta no es JSON:")
    print(r.text[:300])
    sys.exit(0)

if isinstance(datos, dict):
    for clave in sorted(datos)[:15]:
        valor = datos[clave]
        if isinstance(valor, (dict, list)):
            valor = type(valor).__name__ + " con %d elementos" % len(valor)
        print("  %-20s %s" % (clave, valor))
else:
    print(datos)

# Enviar JSON es igual de fácil:
#   r = requests.post("http://10.0.2.2:8000/api", json={"sagui": "Tito"})
