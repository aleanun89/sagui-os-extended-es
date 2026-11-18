/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Lectura de los datos originales del juego: archivos DRS, sprites SLP y paletas JASC. */
#include "aoen.h"

static u32 rd32(const u8 *p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }
static int rd16s(const u8 *p) { return (int)(int16_t)((u32)p[0] | ((u32)p[1] << 8)); }

static int cmp_item(const void *a, const void *b) {
	u32 x = ((const DrsItem *)a)->id, y = ((const DrsItem *)b)->id;
	return x < y ? -1 : x > y;
}

int drs_open(Drs *d, const char *path) {
	u8 hdr[64];
	memset(d, 0, sizeof *d);
	d->f = fopen(path, "rb");
	if (!d->f)
		return 0;
	fseek(d->f, 0, SEEK_END);
	d->len = ftell(d->f);
	fseek(d->f, 0, SEEK_SET);
	if (d->len < 64 || fread(hdr, 1, 64, d->f) != 64)
		goto fail;

	u32 nlist = rd32(hdr + 56);
	if (nlist == 0 || nlist > 64)
		goto fail;

	u8 lists[64 * 12];
	if (fread(lists, 12, nlist, d->f) != nlist)
		goto fail;

	/* todo lo que dice el fichero se comprueba con la longitud real, con cuentas de 64 bits */
	unsigned long long total = 0;
	for (u32 i = 0; i < nlist; ++i) {
		unsigned long long off = rd32(lists + i * 12 + 4), n = rd32(lists + i * 12 + 8);
		if (off + n * 12 > (unsigned long long)d->len)
			goto fail;
		total += n;
	}
	if (total > (unsigned long long)d->len / 12)
		goto fail;

	d->items = (DrsItem *)malloc(sizeof(DrsItem) * (size_t)(total ? total : 1));
	if (!d->items)
		goto fail;
	for (u32 i = 0; i < nlist; ++i) {
		u32 off = rd32(lists + i * 12 + 4), n = rd32(lists + i * 12 + 8);
		fseek(d->f, (long)off, SEEK_SET);

		for (u32 j = 0; j < n && (unsigned long long)d->count < total; ++j) {
			u8 it[12];
			if (fread(it, 1, 12, d->f) != 12)
				break;
			d->items[d->count].id = rd32(it);
			d->items[d->count].offset = rd32(it + 4);
			d->items[d->count].size = rd32(it + 8);
			++d->count;
		}
	}

	qsort(d->items, (size_t)d->count, sizeof(DrsItem), cmp_item);
	return 1;

fail:
	free(d->items);
	fclose(d->f);
	memset(d, 0, sizeof *d);
	return 0;
}

void drs_close(Drs *d) {
	if (d->f)
		fclose(d->f);
	free(d->items);
	memset(d, 0, sizeof *d);
}

static const DrsItem *drs_find(Drs *d, u32 id) {
	DrsItem key = { id, 0, 0 };
	return (const DrsItem *)bsearch(&key, d->items, (size_t)d->count, sizeof(DrsItem), cmp_item);
}

#define DRS_MAX_ITEM (64u * 1024 * 1024)   /* ningún elemento de un DRS real se acerca a esto */

u8 *drs_read(Drs *d, u32 id, int *size) {
	const DrsItem *it = drs_find(d, id);
	if (!it || !d->f)
		return NULL;
	if ((unsigned long long)it->offset + it->size > (unsigned long long)d->len || it->size > DRS_MAX_ITEM)
		return NULL;

	u8 *buf = (u8 *)malloc(it->size ? it->size : 1);
	if (!buf)
		return NULL;
	fseek(d->f, (long)it->offset, SEEK_SET);
	if (fread(buf, 1, it->size, d->f) != it->size) {
		free(buf);
		return NULL;
	}
	if (size)
		*size = (int)it->size;
	return buf;
}

Slp *slp_load(Drs *d, u32 id) {
	int size = 0;
	u8 *data = drs_read(d, id, &size);
	if (!data || size < 32)
		return NULL;

	Slp *s = (Slp *)calloc(1, sizeof *s);
	s->data = data;
	s->size = size;
	s->nframes = (int)rd32(data + 4);
	if (s->nframes < 0 || s->nframes > 4096 || 32 + (long long)s->nframes * 32 > size)
		s->nframes = 0;
	s->frames = (Sprite **)calloc((size_t)(s->nframes ? s->nframes : 1), sizeof(Sprite *));
	return s;
}

static unsigned cmd_or_next(const u8 *c, int size, int *pos, unsigned n) {
	unsigned v = c[*pos] >> n;
	if (v)
		return v;
	++*pos;
	return *pos < size ? c[*pos] : 0;
}

static Sprite *slp_decode(Slp *s, int index) {
	const u8 *fi = s->data + 32 + index * 32;
	u32 cmd_off = rd32(fi), out_off = rd32(fi + 4);
	int w = (int)rd32(fi + 16), h = (int)rd32(fi + 20);
	int hx = (int)rd32(fi + 24), hy = (int)rd32(fi + 28);

	if (w < 0 || h < 0 || w > 2048 || h > 2048 || (unsigned long long)out_off + (unsigned long long)h * 4 > (unsigned long long)s->size || (unsigned long long)cmd_off + (unsigned long long)h * 4 > (unsigned long long)s->size)
		return NULL;

	Sprite *sp = (Sprite *)calloc(1, sizeof *sp);
	sp->w = w; sp->h = h; sp->hx = hx; sp->hy = hy;
	if (!w || !h)
		return sp;

	sp->px = (u8 *)calloc((size_t)w * h, 1);
	sp->pc = (u8 *)calloc((size_t)w * h, 1);
	if (!sp->px || !sp->pc) { free(sp->px); free(sp->pc); free(sp); return NULL; }

	const u8 *edges = s->data + out_off;
	/* órdenes de dibujo en una copia con ceros detrás: las órdenes truncadas leen 1 o 2 bytes de más sin salir del búfer */
	int csize = s->size - (int)(cmd_off + (u32)h * 4);
	u8 *cmd = (u8 *)calloc((size_t)csize + 8, 1);
	if (!cmd) { free(sp->px); free(sp->pc); free(sp); return NULL; }
	memcpy(cmd, s->data + cmd_off + (u32)h * 4, (size_t)csize);
	int pos = 0;

	for (int y = 0; y < h; ++y) {
		int left = rd16s(edges + y * 4), right = rd16s(edges + y * 4 + 2);
		if (left == -32768 || right == -32768 || left < 0 || left > w || right < 0)
			continue;

		int line = w - left - right;
		int x = left;
		u8 *row = sp->px + (size_t)y * w;
		u8 *prow = sp->pc + (size_t)y * w;
		int endx = left + line;

		for (int i = left; i <= endx && pos < csize; ++i, ++pos) {
			u8 bc = cmd[pos];
			unsigned count = 0;

			switch (bc & 0xf) {
			case 0x0f: /* fin de fila */
				i = endx;
				continue;
			case 0x0a: /* relleno con el color del jugador */
				count = cmd_or_next(cmd, csize, &pos, 4);
				++pos;
				for (; count && x < w; --count, ++x) { row[x] = cmd[pos]; prow[x] = 1; }
				break;
			case 0x07: /* relleno de bloque */
				count = cmd_or_next(cmd, csize, &pos, 4);
				++pos;
				for (; count && x < w; --count) row[x++] = cmd[pos];
				break;
			case 0x06: /* relleno grande con el color del jugador */
				count = cmd_or_next(cmd, csize, &pos, 4);
				for (; count && x < w && pos + 1 < csize; --count, ++x) { row[x] = cmd[++pos]; prow[x] = 1; }
				break;
			case 0x03: /* salto grande */
				count = cmd[++pos];
				x += (int)count;
				continue;
			case 0x02: /* relleno grande */
				count = ((unsigned)(cmd[pos] & 0xf0) << 4) + cmd[pos + 1];
				++pos;
				for (; count && x < w && pos + 1 < csize; --count) row[x++] = cmd[++pos];
				break;
			case 0x0d: case 0x0b: case 0x09: case 0x05: case 0x01: /* salto */
				count = cmd_or_next(cmd, csize, &pos, 2);
				x += (int)count;
				break;
			case 0x0c: case 0x08: case 0x04: case 0x00: /* relleno */
				count = cmd_or_next(cmd, csize, &pos, 2);
				for (; count && x < w && pos + 1 < csize; --count) row[x++] = cmd[++pos];
				break;
			case 0x0e:
				if ((bc & 0xf0) == 0x40 || (bc & 0xf0) == 0x60) {
					if (x < w && pos + 1 < csize)
						row[x++] = cmd[++pos];
					continue;
				}
				/* sin break: continúa en el siguiente caso */
			default: /* desconocido, salta al final de la fila */
				while (pos < csize && cmd[pos] != 0x0f)
					++pos;
				i = endx;
				break;
			}
		}
	}

	free(cmd);
	return sp;
}

Sprite *slp_frame(Slp *s, int index) {
	if (!s || s->nframes <= 0)
		return NULL;
	index = ((index % s->nframes) + s->nframes) % s->nframes;
	if (!s->frames[index])
		s->frames[index] = slp_decode(s, index);
	return s->frames[index];
}

int pal_load(Drs *d, u32 id, u32 *rgb) {
	int size = 0;
	u8 *data = drs_read(d, id, &size);
	if (!data)
		return 0;

	char *txt = (char *)realloc(data, (size_t)size + 1);
	if (!txt) { free(data); return 0; }
	txt[size] = 0;

	/* salta las líneas "JASC-PAL", la versión y el número */
	char *p = txt;
	for (int i = 0; i < 3; ++i) {
		char *nl = strchr(p, '\n');
		if (!nl) { free(txt); return 0; }
		p = nl + 1;
	}

	for (int i = 0; i < 256; ++i) {
		unsigned r, g, b;
		if (sscanf(p, "%u %u %u", &r, &g, &b) != 3) { free(txt); return 0; }
		rgb[i] = (r << 16) | (g << 8) | b;
		char *nl = strchr(p, '\n');
		if (!nl) break;
		p = nl + 1;
	}

	free(txt);
	return 1;
}
