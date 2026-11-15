// SPDX-License-Identifier: GPL-2.0-or-later
/* nvme.h - controlador NVMe (D265). */
#ifndef SAGUI_NVME_H
#define SAGUI_NVME_H

/* Busca los controladores NVMe (PCI clase 01h, subclase 08h, interfaz 02h), los enciende
 * y registra cada namespace activo como un disco ("nvme0", "nvme1"...). Devuelve cuántos
 * discos ha registrado. Nunca se queda colgado: todas las esperas tienen plazo. */
int nvme_iniciar(void);

/* Cuántos controladores NVMe se han visto en el PCI y cuántos no se han podido usar. */
int nvme_controladores(void);
int nvme_sem_suporte(void);

#endif
