# Opciones del kernel desde el sistema: `kconfig` (D353)

Algunas opciones del kernel (memoria, swap, disco y frecuencia de la CPU) se pueden cambiar desde SaguiOS. Se guardan en la
cabecera de arranque del disco del que arrancó el sistema y **se aplican al reiniciar** (el gobernador de la frecuencia,
también al momento). Hace falta ser administrador. Desde un CD no se puede guardar.

## En la terminal

```
kconfig                                   # lista: valor de ahora, de la imagen, guardado y si cambia al reiniciar
admin kconfig pae=no dma=no               # guarda (varias a la vez)
admin kconfig cache_max_mb=64 vaciado_s=5
admin kconfig gobernador=ahorro           # rendimiento | ahorro | demanda | auto
admin kconfig dma=defecto                 # esa opción vuelve al valor de la imagen
admin kconfig --por-defecto               # borra todo lo guardado
kconfig -h                                # qué hace cada opción y su rango
```

| Opción | Valores | Qué hace |
|---|---|---|
| `pae` | si, no | paginación PAE (memoria por encima de 4 GB) |
| `nx` | si, no | protección NX (solo con PAE) |
| `cache_disco` | si, no | caché de disco |
| `cache_alta` | si, no | caché en la memoria alta (con «no», en la baja, hasta 64 MB) |
| `escritura_diferida` | si, no | lo escrito se graba unos segundos después |
| `dma` | si, no | DMA de los discos IDE (con «no», PIO) |
| `cache_max_mb` | auto, 4-1024 | tope de la caché (solo puede bajar el automático) |
| `sucios_max_mb` | auto, 1-16 | máximo sin grabar en la caché |
| `vaciado_s` | auto, 1-30 | segundos que espera un bloque escrito (3 de serie) |
| `frecuencia` | si, no | control de frecuencia de la CPU |
| `gobernador` | auto, rendimiento, ahorro, demanda | política del control de frecuencia (manda sobre `frecuencia --guardar`) |
| `swap` | si, no | swap comprimida en la RAM (D352) |
| `swap_mb` | auto, 4-1024 | tope de la swap comprimida (automático: la mitad de la RAM) |

## En Configuración

**Configuración > Pantalla > Kernel**: las mismas opciones, con su valor de ahora y el del próximo arranque (en naranja
con `*` si cambia). «Cambiar» pasa al siguiente valor, «De la imagen» quita lo guardado de esa opción y «Valores de la
imagen» lo borra todo. Pide la contraseña de administrador.

## Si el sistema no arranca

Mantén **Ctrl** pulsado al arrancar: el **modo rescate** no aplica nada de lo guardado (arranca con los valores de la
imagen). En su shell de root, `kconfig --por-defecto` lo borra; `reiniciar` vuelve al arranque normal.

Un valor guardado fuera de rango se ignora con un aviso en el registro (`dmesg | grep KCONF`).
