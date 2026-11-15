#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# run_in_86box.sh - arranca SaguiOS a mano en un PC antiguo emulado por 86Box (D301), con ventana,
# para trastear. Es el equivalente de run_in_qemu.sh para las máquinas de la época.
#
# Uso: ./run_in_86box.sh [MAQUINA] [opciones]
#   MAQUINA             preset de tools/86box/maquinas/ (por defecto: p2-voodoo3)
#                         p2-voodoo3  Pentium II 350, 440BX, 128 MB, Voodoo3, SB16, NE2000 PCI
#                         p2-450-voodoo3500  Pentium II 450, 440BX, 256 MB, Voodoo3 3500, SB16, RTL8139 (red con DHCP)
#                         p3-rage     Pentium II 450 (P3B-F), 256 MB, ATI Rage II, ES1371, NE2000 PCI
#                         p1-s3       Pentium MMX 200, 430HX, 64 MB, S3 Trio64, SB16, NE2000 ISA
#                         k6-matrox   K6-2 350, VIA MVP3, 128 MB, Matrox Mystique, SB16, NE2000 PCI
#   --imagen ARCHIVO    imagen raw de SaguiOS (BIOS, no UEFI); por defecto build/saguios.img
#   --lista             enseña los presets y sale
#   --sin-f1            no pulsar F1 en el aviso de CMOS de la BIOS (para entrar tú en el SETUP)
#   --windows           usa el 86Box NATIVO DE WINDOWS (desde WSL, por la interoperabilidad) en vez del de
#                       Linux: en Windows el ratón se captura bien; con WSLg va a saltos (docs/86box.md,
#                       «Ratón»). Se instala una vez con: bash tools/86box_instalar.sh --windows
#                       Todo vive en %LOCALAPPDATA%\saguitools\86box\ (vm\manual-<máquina>\), fuera del
#                       repositorio. El serie (COM1) va a serie.txt allí; se lee con «tail -f» por /mnt/c.
#
# Ratón: clic en la ventana para capturarlo; Ctrl+End (o el botón central) lo suelta.
#
# La imagen se copia a build/86box/manual-<máquina>/disco.img: la original no se toca y los
# cambios de una sesión se pierden en la siguiente. El serie (COM1) queda en
# build/86box/manual-<máquina>/serie.txt (sirve para «tail -f»). Se cierra cerrando la ventana.
#
# Estado (issues #33-#38): p2-voodoo3 y p2-450-voodoo3500 llegan al escritorio; las demás se paran en la etapa 2
# o reinician. Las ROM de las BIOS NO SE SUBEN NUNCA (viven en ~/.local/saguitools/86box/roms).
# Instalar 86Box la primera vez: bash tools/86box_instalar.sh
set -eu
RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MAQUINA=p2-voodoo3
IMAGEN="$RAIZ/build/saguios.img"
EXTRA=()
WINDOWS=0
SIN_F1=0
# Dispositivo de serie a fichero de 86Box (interno y sección de configuración); ver docs/86box.md.
SERIE_DISPOSITIVO=file
SERIE_SECCION="File (COM)"

while [ $# -gt 0 ]; do
    case "$1" in
        --imagen) IMAGEN="${2:?falta el archivo de --imagen}"; shift 2 ;;
        --lista) for f in "$RAIZ"/tools/86box/maquinas/*.cfg; do
                     n="$(basename "$f" .cfg)"; d="$(sed -n 2p "$f" | sed 's/^; *//')"
                     printf '  %-12s %s\n' "$n" "$d"
                 done; exit 0 ;;
        --sin-f1) EXTRA+=(--sin-f1); SIN_F1=1; shift ;;
        --windows) WINDOWS=1; shift ;;
        -h|--help) sed -n 3,30p "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
        -*) echo "opción desconocida: $1 (ver --help)" >&2; exit 2 ;;
        *) MAQUINA="$1"; shift ;;
    esac
done

# ------------------------------------------------------------------ modo Windows (--windows)
# 86Box.exe corre en Windows (interoperabilidad de WSL), con el mismo preset y la misma imagen.
# No pasa por tools/turno.sh ni cuenta como QEMU de pruebas: es uso manual del mantenedor.
if [ "$WINDOWS" = 1 ]; then
    [ -f "$RAIZ/tools/86box/maquinas/$MAQUINA.cfg" ] || {
        echo "no existe el preset «$MAQUINA» (./run_in_86box.sh --lista)" >&2; exit 1; }
    [ -f "$IMAGEN" ] || { echo "no existe la imagen $IMAGEN (make, o --imagen ARCHIVO)" >&2; exit 1; }
    command -v cmd.exe >/dev/null 2>&1 || {
        echo "sin interoperabilidad con Windows (cmd.exe no responde): revisa [interop] en /etc/wsl.conf" >&2; exit 1; }
    LAD="$(cd /mnt/c && cmd.exe /c 'echo %LOCALAPPDATA%' 2>/dev/null | tr -d '\r')"
    [ -n "$LAD" ] || { echo "cmd.exe no da %LOCALAPPDATA%" >&2; exit 1; }
    WB="$(wslpath -u "$LAD")/saguitools/86box"
    [ -f "$WB/86Box.exe" ] && [ -d "$WB/roms/machines" ] || {
        echo "86Box de Windows no está instalado: bash tools/86box_instalar.sh --windows" >&2; exit 1; }
    VM="$WB/vm/manual-$MAQUINA"
    mkdir -p "$VM/capturas"
    # El disco se copia siempre (la imagen original no se toca); la NVR/CMOS de la BIOS se conserva
    # entre sesiones (la carpeta no se borra), así el aviso de CMOS solo sale la primera vez.
    cp --sparse=always "$IMAGEN" "$VM/disco.img"
    : > "$VM/serie.txt"
    PISTAS=$(( $(stat -c %s "$VM/disco.img") / (63 * 16 * 512) ))
    cfg="$(cat "$RAIZ/tools/86box/maquinas/$MAQUINA.cfg")"
    cfg="${cfg//@IMAGEN@/$(wslpath -w "$VM/disco.img")}"
    cfg="${cfg//@PISTAS@/$PISTAS}"
    cfg="${cfg//@CAPTURAS@/$(wslpath -w "$VM/capturas")}"
    # En Windows no hay pty: COM1 va al dispositivo «File (COM)» de 86Box (escribe en bloques,
    # puede llegar con algo de retraso).
    cfg="$(printf '%s\n' "$cfg" | sed -e 's/^serial1_device = .*/serial1_device = SERIE_ARCHIVO/' \
        -e '/^\[Serial Passthrough (COM) #1\]/,/^$/d')"
    cfg="${cfg//SERIE_ARCHIVO/$SERIE_DISPOSITIVO}"
    printf '%s\n' "$cfg" > "$VM/86box.cfg"
    printf '\n[%s #1]\npath = %s\n' "$SERIE_SECCION" "$(wslpath -w "$VM/serie.txt")" >> "$VM/86box.cfg"
    echo "86Box (Windows): $MAQUINA con $IMAGEN"
    echo "carpeta: $VM"
    echo "serie:   $VM/serie.txt   (tail -f en otra terminal)"
    echo "ratón: clic en la ventana para capturarlo; Ctrl+End (o botón central) lo suelta."
    echo "Uso manual: no pasa por tools/turno.sh ni cuenta como QEMU de pruebas. Se cierra cerrando la ventana."
    # F1 en el aviso de CMOS (primer arranque): se pulsa desde PowerShell solo hasta que el serie
    # tenga texto (máximo 30 s). Con --sin-f1, nada.
    WIN_VM="$(wslpath -w "$VM")"; WIN_EXE="$(wslpath -w "$WB/86Box.exe")"; WIN_ROMS="$(wslpath -w "$WB/roms")"
    PS1CODE='
$p = Start-Process -FilePath $env:S86_EXE -ArgumentList @("-P", $env:S86_VM, "-R", $env:S86_ROMS, "-N", ($env:S86_VM + "\86box.cfg")) -PassThru
Write-Host ("PID de Windows: " + $p.Id)
if ($env:S86_F1 -eq "1") {
    $ws = New-Object -ComObject WScript.Shell
    $fin = (Get-Date).AddSeconds(30)
    while ((Get-Date) -lt $fin -and -not $p.HasExited -and (Get-Item ($env:S86_VM + "\serie.txt")).Length -eq 0) {
        Start-Sleep -Seconds 2
        if ($ws.AppActivate($p.Id)) { $ws.SendKeys("{F1}") }
    }
}
$p.WaitForExit()'
    cd /mnt/c
    S86_EXE="$WIN_EXE" S86_VM="$WIN_VM" S86_ROMS="$WIN_ROMS" S86_F1=$([ "$SIN_F1" = 1 ] && echo 0 || echo 1) \
    WSLENV="S86_EXE:S86_VM:S86_ROMS:S86_F1" \
        exec powershell.exe -NoProfile -Command "$PS1CODE"
fi

[ -x "$HOME/.local/saguitools/86box/bin/86box" ] || {
    echo "86Box no está instalado: bash tools/86box_instalar.sh" >&2; exit 1; }
[ -f "$RAIZ/tools/86box/maquinas/$MAQUINA.cfg" ] || {
    echo "no existe el preset «$MAQUINA» (./run_in_86box.sh --lista)" >&2; exit 1; }
[ -f "$IMAGEN" ] || { echo "no existe la imagen $IMAGEN (make, o --imagen ARCHIVO)" >&2; exit 1; }
[ -n "${DISPLAY:-}" ] || { echo "sin DISPLAY: hace falta una pantalla (WSLg o X11)" >&2; exit 1; }

SALIDA="$RAIZ/build/86box/manual-$MAQUINA"
if [ -n "${WSL_DISTRO_NAME:-}" ] && [ -n "${WAYLAND_DISPLAY:-}${WSL_INTEROP:-}" ]; then
    echo "Aviso: con WSLg el ratón de 86Box va a saltos (no admite bien el bloqueo del puntero)." >&2
    echo "       Para trastear usa  ./run_in_86box.sh --windows  (86Box nativo de Windows). Soltar el ratón: Ctrl+End." >&2
fi
echo "86Box: $MAQUINA con $IMAGEN"
echo "serie: $SALIDA/serie.txt   (tail -f en otra terminal)"
# Un día entero encendida como mucho; termina en cuanto se cierra la ventana de 86Box.
# Pasa por tools/turno.sh como un QEMU de pruebas más (cuenta para el límite de carga).
exec "$RAIZ/tools/86box.sh" "$MAQUINA" "$IMAGEN" --segundos 86400 --salida "$SALIDA" ${EXTRA[@]+"${EXTRA[@]}"}
