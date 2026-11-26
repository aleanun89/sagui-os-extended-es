// SPDX-License-Identifier: GPL-3.0-only
/* image-reader.cpp - PNGReader y JPEGReader con stb_image en lugar de libpng y libjpeg (D481).
 *
 * Mismo contrato que el image-reader.cpp de glmark2: nextRow() entrega las filas de arriba abajo con pixelBytes() = 3
 * (RGB) o 4 (RGBA; el gris con alfa y los de paleta con transparencia también). Todo el archivo se lee a memoria. */
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_THREAD_LOCALS
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb_image.h"

#include <cstring>
#include <memory>
#include <vector>

#include "image-reader.h"
#include "log.h"
#include "util.h"

/* Lector común: PNG y JPEG se distinguen solo por el nombre de la clase. */
struct StbReaderPrivate
{
    StbReaderPrivate() : pixels(0), w(0), h(0), bytes(3), row(0), error(true) {}
    ~StbReaderPrivate() { if (pixels) stbi_image_free(pixels); }

    bool load(const std::string& filename)
    {
        const std::unique_ptr<std::istream> is(Util::get_resource(filename));
        if (!is || !(*is)) {
            Log::error("Cannot open file %s!\n", filename.c_str());
            return false;
        }
        std::vector<unsigned char> data;
        char buf[8192];
        for (;;) {
            is->read(buf, sizeof buf);
            std::streamsize n = is->gcount();
            if (n <= 0)
                break;
            data.insert(data.end(), buf, buf + n);
        }
        int comp = 0;
        if (!stbi_info_from_memory(data.data(), (int)data.size(), &w, &h, &comp)) {
            Log::error("Cannot read image %s: %s\n", filename.c_str(), stbi_failure_reason());
            return false;
        }
        /* RGB si no hay alfa (gris incluido), RGBA si lo hay: como PNG_TRANSFORM_GRAY_TO_RGB | EXPAND */
        bytes = (comp == 1 || comp == 3) ? 3 : 4;
        pixels = stbi_load_from_memory(data.data(), (int)data.size(), &w, &h, &comp, bytes);
        if (!pixels) {
            Log::error("Cannot decode image %s: %s\n", filename.c_str(), stbi_failure_reason());
            return false;
        }
        return true;
    }

    unsigned char *pixels;
    int w, h, bytes;
    unsigned row;
    bool error;
};

struct PNGReaderPrivate : StbReaderPrivate {};
struct JPEGReaderPrivate : StbReaderPrivate {};

template <class P> static bool next_row(P *p, unsigned char *dst)
{
    if (!p->pixels || p->row >= (unsigned)p->h)
        return false;
    memcpy(dst, p->pixels + (size_t)p->row * p->w * p->bytes, (size_t)p->w * p->bytes);
    p->row++;
    return true;
}

PNGReader::PNGReader(const std::string& filename) : priv_(new PNGReaderPrivate())
{
    Log::debug("Reading PNG file %s\n", filename.c_str());
    priv_->error = !init(filename);
}
PNGReader::~PNGReader() { finish(); delete priv_; }
bool PNGReader::error() { return priv_->error; }
bool PNGReader::nextRow(unsigned char *dst) { return next_row(priv_, dst); }
unsigned int PNGReader::width() const { return priv_->w; }
unsigned int PNGReader::height() const { return priv_->h; }
unsigned int PNGReader::pixelBytes() const { return priv_->bytes; }
bool PNGReader::init(const std::string& filename) { return priv_->load(filename); }
void PNGReader::finish() {}

JPEGReader::JPEGReader(const std::string& filename) : priv_(new JPEGReaderPrivate())
{
    Log::debug("Reading JPEG file %s\n", filename.c_str());
    priv_->error = !init(filename);
}
JPEGReader::~JPEGReader() { finish(); delete priv_; }
bool JPEGReader::error() { return priv_->error; }
bool JPEGReader::nextRow(unsigned char *dst) { return next_row(priv_, dst); }
unsigned int JPEGReader::width() const { return priv_->w; }
unsigned int JPEGReader::height() const { return priv_->h; }
unsigned int JPEGReader::pixelBytes() const { return priv_->bytes; }
bool JPEGReader::init(const std::string& filename) { return priv_->load(filename); }
void JPEGReader::finish() {}
