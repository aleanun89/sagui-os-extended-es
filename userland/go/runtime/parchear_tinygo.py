#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Parchea una copia de src/runtime/runtime_unix.go de TinyGo (D364).

SaguiOS no tiene paginación a demanda dentro de lo reservado: el montón del
recolector se reserva entero (sin asignar páginas) y se asignan a medida que
crece. El resto del runtime de «Linux con libc» de TinyGo se usa tal cual.
Uso: parchear_tinygo.py <runtime_unix.go>. Falla en voz alta si el fichero ya
no tiene el texto esperado (otra versión de TinyGo)."""
import io
import sys

ruta = sys.argv[1]
s = io.open(ruta, encoding='utf-8').read()

def cambiar(viejo, nuevo):
    global s
    if s.count(viejo) != 1:
        sys.exit('parchear_tinygo: no encuentro (o está repetido) el texto:\n' + viejo)
    s = s.replace(viejo, nuevo)

# 1. Funciones de la capa C (glue.c).
cambiar('//export abort\nfunc abort()\n',
        '''//export abort
func abort()

// Reserva y asignación del montón en SaguiOS (userland/go/runtime/glue.c).
//
//export sg_heap_reservar
func sg_heap_reservar(bytes uint32) unsafe.Pointer

//export sg_heap_asignar
func sg_heap_asignar(addr unsafe.Pointer, bytes uint32) int32
''')

# 2. Montón: 256 MB de direcciones, reservadas con sg_heap_reservar.
cambiar('heapMaxSize = 1 * 1024 * 1024 * 1024 // 1GB for the entire heap',
        'heapMaxSize = 256 * 1024 * 1024 // SaguiOS: 256 MB de direcciones (D364)')
cambiar('addr := mmap(nil, heapMaxSize, flag_PROT_READ|flag_PROT_WRITE, flag_MAP_PRIVATE|flag_MAP_ANONYMOUS, -1, 0)',
        'addr := sg_heap_reservar(uint32(heapMaxSize))')
cambiar('		heapStart = uintptr(addr)\n		heapEnd = heapStart + heapSize\n',
        '''		heapStart = uintptr(addr)
		heapEnd = heapStart + heapSize
		if sg_heap_asignar(addr, uint32(heapSize)) != 0 {
			runtimeFatal("cannot allocate heap memory")
		}
''')

# 3. Crecer el montón: asignar las páginas nuevas antes de usarlas.
cambiar('''	heapSize = (heapSize * 4 / 3) &^ 4095 // grow by around 33%
	if heapSize > heapMaxSize {
		heapSize = heapMaxSize
	}
	setHeapEnd(heapStart + heapSize)''',
        '''	viejo := heapSize
	heapSize = (heapSize * 4 / 3) &^ 4095 // grow by around 33%
	if heapSize > heapMaxSize {
		heapSize = heapMaxSize
	}
	if sg_heap_asignar(unsafe.Pointer(heapStart+viejo), uint32(heapSize-viejo)) != 0 {
		heapSize = viejo // sin memoria: el recolector lo tratará como montón lleno
		return false
	}
	setHeapEnd(heapStart + heapSize)''')

io.open(ruta, 'w', encoding='utf-8').write(s)
