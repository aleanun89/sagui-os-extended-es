# SPDX-License-Identifier: GPL-2.0-or-later
# red.py - un servidor web mínimo con sockets.
# Ejecuta con: python /ejemplos/red.py [puerto]
# Y visítalo desde otro equipo (o desde el anfitrión de QEMU con hostfwd).
# Cada visita muestra la hora y cuántas visitas lleva. Ctrl+C lo para.
import socket
import sys
import time

puerto = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
servidor = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
servidor.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
servidor.bind(("", puerto))
servidor.listen(5)
print("Servidor web en el puerto", puerto, "(Ctrl+C para salir)")

visitas = 0
while True:
    conexion, (ip, _) = servidor.accept()
    conexion.settimeout(10)
    try:
        pedido = conexion.recv(2048).decode().split("\r\n")[0]
        visitas += 1
        print("Visita %d de %s: %s" % (visitas, ip, pedido))
        cuerpo = ("<html><head><meta charset='utf-8'><title>SaguiOS</title></head><body>"
                  "<h1>¡Hola desde SaguiOS!</h1><p>Son las %s. Visitas: %d.</p></body></html>"
                  % (time.strftime("%H:%M:%S"), visitas)).encode()
        conexion.sendall(b"HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
                         b"Content-Length: %d\r\nConnection: close\r\n\r\n" % len(cuerpo))
        conexion.sendall(cuerpo)
    except OSError as e:
        print("Error:", e)
    conexion.close()
