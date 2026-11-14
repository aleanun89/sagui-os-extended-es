---
name: base-vs-apps-particiones
description: "Dirección y política del mantenedor: solo la base en el ramdisk; apps nuevas y no esenciales en la partición SAGUI-APPS (lectura y escritura) montada en /apps"
metadata:
  node_type: memory
  type: project
  originSessionId: 70ab6765-4570-4f0d-a587-96a40f1d4263
  modified: 2026-11-12T18:15:52.457Z
---

El mantenedor (2026-11-12) separa **base** (lo esencial: shell, escritorio, archivos, discos, instalador, config, bibliotecas comunes → ramdisk en RAM) de lo **no esencial** (apps grandes, juegos, datos → particiones montadas). Copiar juegos/apps a RAM al arrancar le parece un desperdicio.

**Política (2026-11-12):** las **apps nuevas** y las **no fundamentales** van a la partición **`SAGUI-APPS`**, montada en `/apps`, cuando se compila con la opción de APPS. Esa partición es de **lectura y escritura** (para poder instalar apps extra en el futuro), a diferencia de `SAGUI-JOGOS` (ext2 solo lectura en `/jogos/opentyrian`, D181). Primera en ir ahí: NetSurf (D184). Las fuentes de terceros grandes (NetSurf, 90 MB) no se guardan en git: guion de descarga con versión fija y SHA-256.

**Why:** ahorra RAM y tiempo de arranque (la BIOS lee todo el ramdisk), clave en portátiles antiguos (Amilo T2400); y permite instalar apps después.

**How to apply:** al añadir un programa nuevo, preguntarse si es base; si no, va a `/apps`. Montaje por etiqueta, y el sistema arranca igual (sin esas apps, ocultas en el menú) si falta la partición. El instalador (D179) crea las mismas particiones. Meta de imagen 38 MB desde la v7.0b.

Relacionado: [[pruebas-por-capa]]
