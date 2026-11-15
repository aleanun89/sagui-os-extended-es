#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# run_in_qemu.sh - arranca SaguiOS en QEMU con ventana.
#
# SaguiOS es una imagen de disco (build/saguios.img) que arranca por BIOS como
# disco duro o como pendrive USB, igual que en el Lenovo G470. Para los PC que
# no arrancan desde USB hay también una ISO de CD (make iso, D077): --cd.
#
# Uso: ./run_in_qemu.sh [opciones]
#   --imagen ARCHIVO    arranca esa imagen en vez de build/saguios.img (p. ej. una de
#                       entrega/latest; un .img.xz se descomprime antes en build/)
#   --usb               arranca como pendrive en un controlador USB 2.0 (EHCI), como en el G470
#   --cd                arranca desde el CD (build/saguios.iso, la crea con make iso si falta)
#   --pendrive ARCHIVO  conecta además un pendrive de datos (imagen FAT32) en USB
#   --mem MB            memoria RAM (por defecto: 2048, todo o que o SO sabe gerir)
#   --nucleos N         procesadores de la máquina (por defecto: 1). El sistema solo
#                       los usa si la imagen se creó con mkimagem.py --smp (D164)
#   --cpu MODELO        modelo de CPU de QEMU (p. ej. pentium3 o athlon,-sse2 para probar sin
#                       SSE2; pentium2 sin SSE)
#   --gpu MODELO       tarjeta gráfica: std (la de siempre), virtio o vmware
#   --vram MB          memoria de vídeo de la tarjeta (por defecto: 64; solo con --gpu std)
#   --sonido            sonido por PulseAudio (WSLg): el altavoz del PC (bip) y una tarjeta
#                       Intel HDA (reproducir, volumen, CHIP-8, sagui.Sonido)
#   --ac97              con --sonido, una tarjeta AC'97 en lugar de la HDA
#   --raton-relativo    ratón PS/2 solo relativo, como en el G470 (por defecto, en QEMU
#                       el cursor de SaguiOS sigue al de la ventana: vmmouse, D035)
#   --usb-real VID:PID  pasa a SaguiOS un dispositivo USB real del PC (por usbipd en WSL) en el
#                       controlador USB 2.0; se puede repetir. Requiere permiso en /dev/bus/usb
#                       (regla de udev, ver docs/usb-real.md)
#   --wifi-usb          atajo: el pincho Wi-Fi RTL8188EU (0bda:8179 y su modo CD 1111:1111)
#   --xhci              el pendrive de datos, los dispositivos reales y el teclado/ratón USB van
#                       por un controlador USB 3.0 (xHCI, qemu-xhci) en vez del EHCI (D214)
#   --uhci / --ohci     los dispositivos USB de datos van por un controlador USB 1.1 (UHCI, de Intel/VIA,
#                       2 puertos; OHCI, de AMD/NVIDIA, 3 puertos) en vez del EHCI (D267)
#   --hid-usb           además del PS/2, un teclado y una tableta (ratón absoluto) USB
#   --serie             muestra el log del kernel (puerto serie) en este terminal
#   --vnc               sin ventana: servidor VNC en localhost:5900
#   --guardar           los cambios en el disco de arranque se guardan (por defecto se descartan)
#   --compilar          ejecuta make antes de arrancar
#   --smp               enciende todos los procesadores (imagen con mkimagem.py --smp,
#                       D164); sin --nucleos, la máquina tiene 4 (o los que haya, si son menos)
#   --ioapic            interrupciones por el IOAPIC (D166); con --smp, repartidas entre CPU (D168)
#   --planificador P    planificador al arrancar: rr, afinidad, equilibrio o prioridad (D174)
#   --uacpi             intérprete de AML (uACPI) e indicador de batería (D192; desactivado
#                       por defecto)
#   (con --smp, --ioapic, --planificador o --uacpi se arranca build/saguios-opciones.img, creada
#    al momento a partir de la compilación; build/saguios.img no se toca)
#   --uefi [x64|ia32]   arranca por UEFI con el firmware OVMF (por defecto x64) en vez de por
#                       BIOS: la imagen híbrida (D264) lleva la ESP con BOOTX64.EFI y BOOTIA32.EFI.
#                       OVMF: sudo apt install ovmf ovmf-ia32, o bash tools/entorno-local.sh
#   --sin-kvm           sin aceleración KVM (por defecto se usa si /dev/kvm está disponible)
#   -h, --help          esta ayuda
#
# El log del puerto serie se guarda en build/qemu-serie.log (salvo con --serie).
# El ratón no se captura: el cursor de SaguiOS va donde está el de la ventana. Con
# --raton-relativo hay que hacer clic en la ventana para capturarlo y Ctrl+Alt+G para
# soltarlo. Para salir: cierra la ventana.
set -euo pipefail

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGEM="$RAIZ/build/saguios.img"
MEM=2048
NUCLEOS=1
CPU=""
MODO=ide
PENDRIVE=""
USB_REAL=()
XHCI=0
UHCI=0
OHCI=0
HID_USB=0
GPU=""
VRAM=64
VRAM_PIDO=0
SONIDO=0
TARJETA=hda
RELATIVO=0
SERIE=0
VNC=0
GUARDAR=0
COMPILAR=0
OPC_IMAGEM=()
NUCLEOS_PIDO=0
IMAGEM_PEDIDA=""
UEFI=""

ajuda() { sed -n '2,/^set -e/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'; }

while [ $# -gt 0 ]; do
    case "$1" in
        --imagen) IMAGEM_PEDIDA="${2:?falta el archivo de la imagen}"; shift ;;
        --usb) MODO=usb ;;
        --cd) MODO=cd ;;
        --usb-real) USB_REAL+=("${2:?falta VID:PID}"); shift ;;
        --wifi-usb) USB_REAL+=(0bda:8179 1111:1111) ;;
        --xhci) XHCI=1 ;;
        --uhci) UHCI=1 ;;
        --ohci) OHCI=1 ;;
        --hid-usb) HID_USB=1 ;;
        --pendrive) PENDRIVE="${2:?falta el archivo del pendrive}"; shift ;;
        --mem) MEM="${2:?falta o tamanho em MB}"; shift ;;
        --nucleos) NUCLEOS="${2:?falta o numero de nucleos}"; NUCLEOS_PIDO=1; shift ;;
        --cpu) CPU="${2:?falta el modelo de CPU}"; shift ;;
        --gpu) GPU="${2:?falta la tarjeta gráfica}"; shift ;;
        --vram) VRAM="${2:?falta la memoria de vídeo}"; VRAM_PIDO=1; shift ;;
        --sonido) SONIDO=1 ;;
        --ac97) TARJETA=ac97 ;;
        --raton-relativo) RELATIVO=1 ;;
        --serie) SERIE=1 ;;
        --vnc) VNC=1 ;;
        --guardar) GUARDAR=1 ;;
        --compilar) COMPILAR=1 ;;
        --smp) OPC_IMAGEM+=(--smp) ;;
        --ioapic) OPC_IMAGEM+=(--ioapic) ;;
        --uacpi) OPC_IMAGEM+=(--uacpi) ;;
        --planificador) OPC_IMAGEM+=(--planificador "${2:?falta el planificador}"); shift ;;
        --uefi)
            UEFI=x64
            if [ $# -gt 1 ] && { [ "$2" = x64 ] || [ "$2" = ia32 ]; }; then UEFI="$2"; shift; fi ;;
        --sin-kvm) KVM=0 ;;
        -h|--help) ajuda; exit 0 ;;
        *) echo "Opción desconocida: $1 (usa --help)" >&2; exit 1 ;;
    esac
    shift
done

# La tarjeta gráfica: si no se pide ninguna, se deja la que QEMU escoge (como
# siempre). Si se pide, se pone explícita (con su memoria de vídeo).
[ "$VRAM_PIDO" = 1 ] && [ -z "$GPU" ] && GPU=std
case "$GPU" in
    ""|std|virtio|vmware) ;;
    *) echo "Tarjeta gráfica desconocida: $GPU (usa std, virtio o vmware)" >&2; exit 1 ;;
esac

# QEMU y herramientas: las del sistema o, si no están, las instaladas sin sudo en
# ~/.local/saguitools (ver mejoras/005-entorno-sin-sudo.md).
if [ -f "$HOME/.local/saguitools/env.sh" ]; then
    # shellcheck disable=SC1091
    . "$HOME/.local/saguitools/env.sh"
fi
# D264: el OVMF de 64 bits necesita una CPU de 64 bits (qemu-system-x86_64); BIOS e IA32, la de siempre.
QEMU_BIN=qemu-system-i386
[ "$UEFI" = x64 ] && QEMU_BIN=qemu-system-x86_64
if ! command -v "$QEMU_BIN" >/dev/null; then
    echo "No se encuentra $QEMU_BIN. Instálalo con: sudo apt install qemu-system-x86 qemu-system-gui" >&2
    exit 1
fi
OVMF_CODE=""
if [ -n "$UEFI" ]; then
    # D270: la ISO también arranca por UEFI (EFI.IMG en el catálogo de El Torito).
    PREF=OVMF; [ "$UEFI" = ia32 ] && PREF=OVMF32
    for d in "${SAGUI_OVMF:-}" "$HOME/.local/saguitools/root/usr/share/OVMF" /usr/share/OVMF; do
        if [ -n "$d" ] && [ -f "$d/${PREF}_CODE_4M.fd" ] && [ -f "$d/${PREF}_VARS_4M.fd" ]; then
            OVMF_CODE="$d/${PREF}_CODE_4M.fd"; OVMF_VARS_ORIG="$d/${PREF}_VARS_4M.fd"; break
        fi
    done
    if [ -z "$OVMF_CODE" ]; then
        echo "No se encuentra el OVMF ($PREF). Instálalo con: sudo apt install ovmf ovmf-ia32 (o bash tools/entorno-local.sh)" >&2
        exit 1
    fi
fi

if [ "$COMPILAR" = 1 ] || [ ! -f "$IMAGEM" ]; then
    echo "Compilando SaguiOS..."
    make -C "$RAIZ" -j"$(nproc)"
fi
# --smp, --ioapic, --planificador, --uacpi: son opciones de la cabecera de la imagen.
# Se crea otra imagen con ellas (en un segundo, con lo ya compilado) para no
# tocar build/saguios.img, que es la que regenera make.
if [ ${#OPC_IMAGEM[@]} -gt 0 ]; then
    if [ "$MODO" = cd ]; then
        echo "--smp, --ioapic, --planificador y --uacpi no se aplican al CD (--cd)" >&2
        exit 1
    fi
    B="$RAIZ/build"
    IMAGEM="$B/saguios-opciones.img"
    python3 "$RAIZ/tools/mkimagem.py" --mbr "$B/boot/mbr.bin" --estagio2 "$B/boot/estagio2.bin" \
        --kernel "$B/kernel.bin" --kernel-elf "$B/kernel.elf" --ramdisk "$B/ramdisk.sgfs" \
        --saida "$IMAGEM" --vesa ${UEFI:+--uefi "$B/boot/uefi"} "${OPC_IMAGEM[@]}" >/dev/null
    echo "Imagen con ${OPC_IMAGEM[*]}: build/saguios-opciones.img"
    if [ "$NUCLEOS_PIDO" = 0 ] && [[ " ${OPC_IMAGEM[*]} " == *" --smp "* ]]; then
        NUCLEOS=$(nproc); [ "$NUCLEOS" -gt 4 ] && NUCLEOS=4
    fi
fi
# --imagen: otra imagen ya hecha (p. ej. de entrega/latest). Sus opciones (--smp...) ya van
# en su cabecera, así que no se mezcla con --smp/--ioapic/--planificador/--uacpi.
if [ -n "$IMAGEM_PEDIDA" ]; then
    if [ ${#OPC_IMAGEM[@]} -gt 0 ]; then
        echo "--imagen no se combina con --smp, --ioapic, --planificador ni --uacpi (ya van en la imagen)" >&2
        exit 1
    fi
    [ -f "$IMAGEM_PEDIDA" ] || { echo "no existe la imagen: $IMAGEM_PEDIDA" >&2; exit 1; }
    case "$IMAGEM_PEDIDA" in
        *.xz)
            IMAGEM="$RAIZ/build/imagen-pedida.img"
            echo "Descomprimiendo $(basename "$IMAGEM_PEDIDA") en build/imagen-pedida.img..."
            xz -dc "$IMAGEM_PEDIDA" > "$IMAGEM" ;;
        *) IMAGEM="$(cd "$(dirname "$IMAGEM_PEDIDA")" && pwd)/$(basename "$IMAGEM_PEDIDA")" ;;
    esac
    case "$(basename "$IMAGEM_PEDIDA")" in
        *smp*) if [ "$NUCLEOS_PIDO" = 0 ]; then NUCLEOS=$(nproc); [ "$NUCLEOS" -gt 4 ] && NUCLEOS=4; fi ;;
    esac
fi
ISO="$RAIZ/build/saguios.iso"
if [ "$MODO" = cd ] && { [ "$COMPILAR" = 1 ] || [ ! -f "$ISO" ]; }; then
    make -C "$RAIZ" iso
fi

SNAP=",snapshot=on"
[ "$GUARDAR" = 1 ] && SNAP=""

ARGS=(-m "$MEM" -name SaguiOS)
if [ -n "$UEFI" ]; then
    # Las variables del firmware (orden de arranque...) en una copia propia en build/.
    OVMF_VARS="$RAIZ/build/ovmf-vars-$UEFI.fd"
    [ -f "$OVMF_VARS" ] || cp "$OVMF_VARS_ORIG" "$OVMF_VARS"
    ARGS+=(-drive "if=pflash,format=raw,unit=0,readonly=on,file=$OVMF_CODE"
           -drive "if=pflash,format=raw,unit=1,file=$OVMF_VARS")
fi
[ -n "$CPU" ] && ARGS+=(-cpu "$CPU")
# Los procesadores de la máquina: el sistema los usa si la imagen lleva --smp (D164).
[ "$NUCLEOS" -gt 1 ] && ARGS+=(-smp "$NUCLEOS")

# La tarjeta gráfica. Sin --gpu no se toca nada: manda la de siempre. Con
# --gpu se pone explícita ("-vga none" evita que QEMU añada la suya).
# Ojo: VirtIO y VMware necesitan controladores en SaguiOS para sacarles
# partido; la VGA estándar es la que funciona siempre.
if [ -n "$GPU" ]; then
    case "$GPU" in
        std)    ARGS+=(-vga none -device "VGA,vgamem_mb=$VRAM") ;;
        virtio) ARGS+=(-vga none -device virtio-vga) ;;
        vmware) ARGS+=(-vga none -device vmware-svga) ;;
    esac
fi

if [ "$MODO" = usb ]; then
    ARGS+=(-device usb-ehci,id=ehci
           -drive "if=none,id=arranque,format=raw,file=$IMAGEM$SNAP"
           -device usb-storage,bus=ehci.0,drive=arranque,bootindex=0)
elif [ "$MODO" = cd ]; then
    # El CD es de solo lectura: --guardar no cambia nada (D077).
    ARGS+=(-cdrom "$ISO" -boot d)
else
    ARGS+=(-drive "format=raw,file=$IMAGEM,if=ide,index=0$SNAP")
fi

# El controlador de los dispositivos USB de datos: el EHCI (el del arranque por
# USB, si lo hay) o, con --xhci, un xHCI aparte (D214).
if [ "$XHCI" = 1 ]; then
    CTL_USB="qemu-xhci,id=xhci"; BUS_USB=xhci.0; CTL_PUESTO=0
elif [ "$UHCI" = 1 ]; then
    CTL_USB="piix3-usb-uhci,id=uhci"; BUS_USB=uhci.0; CTL_PUESTO=0
elif [ "$OHCI" = 1 ]; then
    CTL_USB="pci-ohci,id=ohci"; BUS_USB=ohci.0; CTL_PUESTO=0
else
    CTL_USB="usb-ehci,id=ehci"; BUS_USB=ehci.0; CTL_PUESTO=0
    [ "$MODO" = usb ] && CTL_PUESTO=1
fi
poner_ctl_usb() {
    if [ "$CTL_PUESTO" = 0 ]; then ARGS+=(-device "$CTL_USB"); CTL_PUESTO=1; fi
}

if [ -n "$PENDRIVE" ]; then
    [ -f "$PENDRIVE" ] || { echo "No existe el pendrive: $PENDRIVE" >&2; exit 1; }
    poner_ctl_usb
    ARGS+=(-drive "if=none,id=datos,format=raw,file=$PENDRIVE"
           -device "usb-storage,bus=$BUS_USB,drive=datos,removable=on")
fi

if [ "${#USB_REAL[@]}" -gt 0 ]; then
    poner_ctl_usb
    for u in "${USB_REAL[@]}"; do
        ARGS+=(-device "usb-host,bus=$BUS_USB,vendorid=0x${u%%:*},productid=0x${u##*:}")
    done
fi

if [ "$HID_USB" = 1 ]; then
    poner_ctl_usb
    ARGS+=(-device "usb-kbd,bus=$BUS_USB" -device "usb-tablet,bus=$BUS_USB")
fi

if [ "$SONIDO" = 1 ]; then
    # Servidor PulseAudio: el de $PULSE_SERVER, o el socket de WSLg
    PA_SRV="${PULSE_SERVER:-}"
    [ -z "$PA_SRV" ] && [ -S /mnt/wslg/PulseServer ] && PA_SRV=unix:/mnt/wslg/PulseServer
    PA_OK=0
    if [ -n "$PA_SRV" ]; then
        PA_SOCK="${PA_SRV#unix:}"
        if [ "$PA_SOCK" != "$PA_SRV" ] && [ ! -S "$PA_SOCK" ]; then
            PA_OK=0
        elif command -v pactl >/dev/null; then
            PULSE_SERVER="$PA_SRV" timeout 3 pactl info >/dev/null 2>&1 && PA_OK=1
        else
            PA_OK=1
        fi
    fi
    if [ "$PA_OK" = 1 ]; then
        export PULSE_SERVER="$PA_SRV"
        ARGS+=(-audiodev "pa,id=audio0,server=$PA_SRV")
    else
        echo "sin PulseAudio: arranco sin sonido" >&2
        ARGS+=(-audiodev none,id=audio0)
    fi
    ARGS+=(-machine pcspk-audiodev=audio0)
    if [ "$TARJETA" = ac97 ]; then
        ARGS+=(-device AC97,audiodev=audio0)
    else
        ARGS+=(-device intel-hda -device hda-duplex,audiodev=audio0)
    fi
else
    # Sin --sonido: el altavoz del PC a un audio nulo, para que QEMU no intente
    # PulseAudio por su cuenta (salía «Failed to initialize PA context»).
    ARGS+=(-audiodev none,id=audio0 -machine pcspk-audiodev=audio0)
fi

[ "$RELATIVO" = 1 ] && ARGS+=(-machine vmport=off)

if [ "$SERIE" = 1 ]; then
    ARGS+=(-serial stdio)
else
    ARGS+=(-serial "file:$RAIZ/build/qemu-serie.log")
fi

if [ "$VNC" = 1 ]; then
    ARGS+=(-display vnc=127.0.0.1:0)
    echo "SaguiOS en VNC: conecta un visor a localhost:5900 (Ctrl+C aquí para apagar)."
elif [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]; then
    ARGS+=(-display sdl)
else
    echo "No hay pantalla gráfica (DISPLAY vacío): uso VNC en localhost:5900." >&2
    ARGS+=(-display vnc=127.0.0.1:0)
fi

# KVM: la CPU del PC ejecuta SaguiOS directamente (10-50 veces más rápido que
# la emulación por software, TCG). Si /dev/kvm no está disponible, TCG.
# "-accel kvm -accel tcg": si KVM falla al arrancar, QEMU sigue con TCG.
ACEL=TCG
if [ "${KVM:-1}" = 1 ] && [ -e /dev/kvm ]; then
    if [ -r /dev/kvm ] && [ -w /dev/kvm ]; then
        ARGS+=(-accel kvm -accel tcg); ACEL=KVM
    elif getent group kvm | grep -qw "$(id -un)" && command -v sg >/dev/null; then
        # Ya estás en el grupo kvm, pero esta sesión del WSL se abrió antes:
        # sg abre una con el grupo, sin tener que reiniciar el WSL.
        ARGS+=(-accel kvm -accel tcg); ACEL="KVM (vía sg)"
        echo "Arrancando SaguiOS ($MODO, ${MEM} MB${CPU:+, CPU $CPU}, $ACEL)..."
        exec sg kvm -c "$(printf '%q ' env ${PULSE_SERVER:+PULSE_SERVER="$PULSE_SERVER"} ${XDG_RUNTIME_DIR:+XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR"} ${LD_LIBRARY_PATH:+LD_LIBRARY_PATH="$LD_LIBRARY_PATH"} "$(command -v "$QEMU_BIN")" "${ARGS[@]}")"
    else
        echo "Sin acceso a /dev/kvm: emulación por software (más lenta). Para KVM: sudo usermod -aG kvm \$USER" >&2
    fi
fi

echo "Arrancando SaguiOS ($MODO, ${MEM} MB${CPU:+, CPU $CPU}${UEFI:+, UEFI $UEFI}, $ACEL)..."
exec "$QEMU_BIN" "${ARGS[@]}"
