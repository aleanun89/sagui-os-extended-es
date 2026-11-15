# 029 - Teclado y ratón USB (HID, OHCI, UHCI, split transactions)

- **Estado:** UHCI y OHCI hechos en la v8.0 (D267, rama `v8.0-usb11`); falta probar en hardware real
- **Prioridad:** media
- **Alta:** 2026-11-07

## Problema / motivación

La pila USB (`kernel/usb/`: `ehci.c`, `usb.c`, `armazenamento.c`) solo habla con
pendrives de alta velocidad por EHCI, con transferencias de control y en bloque. Un teclado o
ratón USB externo deja de funcionar en cuanto arranca el kernel: la emulación PS/2 de la BIOS
se pierde y no hay driver. Faltan:

1. **HID en protocolo de arranque** (*boot protocol*): teclado y ratón con un informe fijo.
2. **Transferencias por interrupción** (sondeo periódico del endpoint).
3. **Acceso a dispositivos de baja o plena velocidad** (USB 1.x), que según la máquina van por:
   - **OHCI**: chipsets AMD como el SB700 del HP dv7 (Athlon X2 QL-64);
   - **UHCI**: chipsets Intel hasta ~2010;
   - **EHCI con split transactions** a través del hub interno con TT: Intel serie 6, como
     el G470. Hoy `ehci.c` no tiene companheiros en esa máquina y no hay TT;
   - **xHCI** en PC de 2012 en adelante (fuera de alcance aquí; mejora aparte).

## Propuesta

- `kernel/usb/hid.c`:
  - SET_PROTOCOL(boot) e informes de 8 bytes (teclado) y 3-4 bytes (ratón, con rueda si la hay);
  - el teclado entra por el mismo camino que el PS/2, con las distribuciones ES/ABNT2, las
    teclas muertas y la repetición automática (USB no la tiene, hay que generarla);
  - el ratón entra por el camino del touchpad, incluido `EV_RODA`;
  - `usb-tablet` (absoluto) como el vmmouse, si es barato.
- Transferencias por interrupción en la capa común (`usb.c`), sondeadas desde el reloj.
- `kernel/usb/ohci.c` y `kernel/usb/uhci.c`: control, bulk e interrupción, enumeración de
  puertos y traspaso desde EHCI (companheiros) para dispositivos lentos.
- `ehci.c`: split transactions para dispositivos LS/FS detrás de hubs con TT.
- De paso, el pendrive de datos funcionaría también en puertos USB 1.1.
- `usb` (comando) e `infopc` muestran los controladores y dispositivos HID.

## Criterios de aceptación

- [x] En QEMU: `usb-kbd` y `usb-mouse` funcionan con `-device piix3-usb-uhci`, `pci-ohci`
      y cedidos por `usb-ehci` al UHCI compañero (hecho en D267, `h267`; el split por hub con TT
      ya estaba en D214)
- [ ] Teclado USB con ES/ABNT2, teclas muertas, Ctrl+C y repetición
- [ ] Ratón USB con botones y rueda; tablet absoluto si se hace
- [x] Conexión y desconexión en caliente
- [ ] El teclado y touchpad PS/2 siguen funcionando a la vez
- [ ] Test automático en `tests/` (QMP envía teclas y movimientos a los dispositivos USB)
- [x] Decisión registrada en `docs/decisoes.md` (D214 y D267)
- [ ] Probado en el HP dv7 (OHCI), en el G470 (split) y en el Amilo (UHCI, `docs/usb11.md`)

## Notas

- Esfuerzo medio (~2.500-3.000 líneas), parecido al hilo de sistemas de archivos.
- Plan por fases:
  1. HID + OHCI + UHCI: todo probable en QEMU, cubre el HP dv7 y los PC Intel antiguos
     (~75 % del trabajo, riesgo bajo).
  2. Split transactions en EHCI para el G470: pequeña, pero QEMU no emula el hub con TT
     de Intel serie 6, así que solo se valida en el portátil.
- Riesgos en hardware real: traspaso desde la BIOS en OHCI, erratas del SB700 y teclados que
  no respetan bien el boot protocol.
- Solo toca `kernel/usb/` y la entrada de teclado/ratón.
- xHCI (USB 3) queda para otra mejora: es bastante más grande.
