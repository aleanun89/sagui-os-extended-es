// SPDX-License-Identifier: GPL-2.0-or-later
/* file_logica.h - el reconocedor de tipos de file (D232): mira la cabecera (los "números mágicos") y, si no es
 * ninguno conocido, decide si es texto (ASCII, UTF-8 o ISO-8859-1) o datos binarios. */
#ifndef FILE_LOGICA_H
#define FILE_LOGICA_H
#include "sagui.h"

static bool fl_pref(const uint8_t *d, int n, const char *m, int largo, int desde)
{
    if (desde + largo > n) return false;
    for (int i = 0; i < largo; i++) if (d[desde + i] != (uint8_t)m[i]) return false;
    return true;
}
#define PREF(m) fl_pref(d, n, m, (int)sizeof(m) - 1, 0)

static uint32_t fl_le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t fl_le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

/* Escribe en t la descripción de los n bytes iniciales del archivo (d). */
static void file_tipo(const uint8_t *d, int n, char *t, int max)
{
    if (n == 0) { copia_texto(t, "vacío", (size_t)max); return; }
    if (PREF("\x7f" "ELF") && n >= 20) {
        static const char *const tipos[] = { "sin tipo", "reubicable", "ejecutable", "objeto compartido", "core" };
        int e = fl_le16(d + 16);
        formatar_texto(t, (size_t)max, "ELF %s-bit %s, %s", d[4] == 2 ? "64" : "32", d[5] == 2 ? "MSB" : "LSB",
                       e < 5 ? tipos[e] : "tipo desconocido");
        if (fl_le16(d + 18) == 3 && d[4] == 1) junta_texto(t, ", Intel 80386", (size_t)max);
        else if (fl_le16(d + 18) == 62) junta_texto(t, ", x86-64", (size_t)max);
        return;
    }
    if (PREF("\x89PNG\r\n\x1a\n")) {
        if (n >= 24) formatar_texto(t, (size_t)max, "imagen PNG, %u x %u", (uint32_t)d[16] << 24 | (uint32_t)d[17] << 16 | (uint32_t)d[18] << 8 | d[19],
                                    (uint32_t)d[20] << 24 | (uint32_t)d[21] << 16 | (uint32_t)d[22] << 8 | d[23]);
        else copia_texto(t, "imagen PNG", (size_t)max);
        return;
    }
    if (PREF("\xff\xd8\xff")) { copia_texto(t, "imagen JPEG", (size_t)max); return; }
    if (PREF("GIF87a") || PREF("GIF89a")) {
        if (n >= 10) formatar_texto(t, (size_t)max, "imagen GIF, %u x %u", fl_le16(d + 6), fl_le16(d + 8));
        else copia_texto(t, "imagen GIF", (size_t)max);
        return;
    }
    if (PREF("BM") && n >= 26) {
        formatar_texto(t, (size_t)max, "imagen BMP, %u x %u", fl_le32(d + 18), fl_le32(d + 22));
        return;
    }
    if (PREF("%PDF-")) { copia_texto(t, "documento PDF", (size_t)max); if (n > 8 && d[5] >= '0') { formatar_texto(t, (size_t)max, "documento PDF, versión %c%c%c", d[5], d[6], d[7]); } return; }
    if (PREF("PK\x03\x04")) { copia_texto(t, "archivo ZIP", (size_t)max); return; }
    if (PREF("PK\x05\x06")) { copia_texto(t, "archivo ZIP vacío", (size_t)max); return; }
    if (PREF("\x1f\x8b")) { copia_texto(t, "datos comprimidos con gzip", (size_t)max); return; }
    if (PREF("BZh")) { copia_texto(t, "datos comprimidos con bzip2", (size_t)max); return; }
    if (PREF("\xfd" "7zXZ")) { copia_texto(t, "datos comprimidos con xz", (size_t)max); return; }
    if (PREF("7z\xbc\xaf\x27\x1c")) { copia_texto(t, "archivo 7-zip", (size_t)max); return; }
    if (fl_pref(d, n, "ustar", 5, 257)) { copia_texto(t, "archivo tar", (size_t)max); return; }
    if (PREF("ID3")) { copia_texto(t, "audio MP3 con etiquetas ID3", (size_t)max); return; }
    if (n >= 2 && d[0] == 0xFF && (d[1] & 0xE0) == 0xE0 && (d[1] & 0x06) != 0) { copia_texto(t, "audio MPEG (MP3)", (size_t)max); return; }
    if (PREF("RIFF") && fl_pref(d, n, "WAVE", 4, 8)) { copia_texto(t, "audio WAV", (size_t)max); return; }
    if (PREF("RIFF") && fl_pref(d, n, "AVI ", 4, 8)) { copia_texto(t, "vídeo AVI", (size_t)max); return; }
    if (PREF("RIFF") && fl_pref(d, n, "WEBP", 4, 8)) { copia_texto(t, "imagen WebP", (size_t)max); return; }
    if (PREF("OggS")) { copia_texto(t, "audio/vídeo Ogg", (size_t)max); return; }
    if (PREF("fLaC")) { copia_texto(t, "audio FLAC", (size_t)max); return; }
    if (PREF("MThd")) { copia_texto(t, "música MIDI", (size_t)max); return; }
    if (fl_pref(d, n, "ftyp", 4, 4)) { copia_texto(t, "vídeo o audio MP4", (size_t)max); return; }
    if (PREF("\x00\x00\x01\xba")) { copia_texto(t, "vídeo MPEG", (size_t)max); return; }
    if (PREF("SQLite format 3")) { copia_texto(t, "base de datos SQLite 3", (size_t)max); return; }
    if (PREF("\xd0\xcf\x11\xe0")) { copia_texto(t, "documento de Microsoft Office antiguo", (size_t)max); return; }
    if (PREF("MZ")) { copia_texto(t, "ejecutable de DOS/Windows", (size_t)max); return; }
    /* ¿texto? */
    bool bin = false, alto = false, utf8 = true, crlf = false;
    int largo = n < 4096 ? n : 4096;
    for (int i = 0; i < largo; i++) {
        uint8_t c = d[i];
        if (c == 0 || (c < 32 && c != '\n' && c != '\r' && c != '\t' && c != '\f' && c != 27 && c != 8)) { bin = true; break; }
        if (c == '\r' && i + 1 < largo && d[i + 1] == '\n') crlf = true;
        if (c >= 128) {
            alto = true;
            int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC2 ? 1 : -1;
            if (extra < 0) utf8 = false;
            else for (int k = 1; k <= extra; k++) { if (i + k >= largo) break; if ((d[i + k] & 0xC0) != 0x80) { utf8 = false; break; } i++; }
        }
    }
    if (bin) { copia_texto(t, "datos", (size_t)max); return; }
    const char *kind = "texto ASCII";
    if (PREF("#!")) {
        int e = 2;
        while (e < n && d[e] != '\n' && e < 60) e++;
        char interp[64];
        int k = 0;
        for (int i = 2; i < e && k < 62; i++) interp[k++] = (char)d[i];
        interp[k] = 0;
        formatar_texto(t, (size_t)max, "guion ejecutable (%s)", interp);
        return;
    }
    if (PREF("<?xml")) kind = "documento XML";
    else if (PREF("<!DOCTYPE html") || PREF("<!doctype html") || PREF("<html") || PREF("<HTML")) kind = "documento HTML";
    else if (alto) kind = utf8 ? "texto UTF-8" : "texto ISO-8859";
    formatar_texto(t, (size_t)max, "%s%s", kind, crlf ? ", con terminadores de línea CRLF" : "");
}

#endif
