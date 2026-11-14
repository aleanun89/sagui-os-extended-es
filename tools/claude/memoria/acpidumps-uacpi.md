---
name: acpidumps-uacpi
description: Volcados ACPI de máquinas reales (uACPI/ACPIDumps) para probar lectores de tablas ACPI de SaguiOS sin el hardware
metadata:
  node_type: memory
  type: reference
  originSessionId: 70ab6765-4570-4f0d-a587-96a40f1d4263
  modified: 2026-11-12T13:48:25.549Z
---

https://github.com/uACPI/ACPIDumps — volcados `acpidump` (texto hex) de ~2700 máquinas reales, CC-BY-4.0 (atribuir en docs/terceros.md si se commitea algún dato). Se descarga por raw.githubusercontent.com (no hay `gh` en la máquina; usar curl con User-Agent genérico).

Útiles para SaguiOS (época del Amilo T2400, Core Duo/945): `Notebook/Dell/Inspiron/Inspiron 6400` (trae el esquema PmRef/CpuPm → CPU0IST/CPU1IST con _PSS/_PSD/_PCT), `Notebook/Toshiba/Satellite/Satellite A135`, `Notebook/Lenovo/ThinkPad/ThinkPad T61` y `X61 Tablet` (Core 2/965). No hay Amilo. El mismo proyecto tiene uACPI, intérprete ACPI embebible (alternativa futura a nuestro lector mínimo, mejora 030).

Relacionado: [[pruebas-por-capa]]
