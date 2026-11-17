/* netfile_sagui.c - sustituto de utility/netfile.c para SaguiOS (D374).
 * El upstream descarga por HTTP con libcurl (metaservidor, modpacks). Aquí no hay curl: todas las
 * operaciones fallan con un mensaje y Freeciv sigue funcionando sin metaservidor. */
#include <fc_config.h>
#include <stdio.h>
#include "netfile.h"

struct section_file *netfile_get_section_file(const char *URL, nf_errmsg cb, void *data)
{
    if (cb) cb("Sin acceso a la red de metaservidores en SaguiOS", data);
    return NULL;
}
bool netfile_download_file(const char *URL, const char *filename, nf_errmsg cb, void *data)
{
    if (cb) cb("Sin descargas en SaguiOS", data);
    return FALSE;
}
struct netfile_post *netfile_start_post(void) { return NULL; }
void netfile_add_form_str(struct netfile_post *post, const char *name, const char *val) { }
void netfile_add_form_int(struct netfile_post *post, const char *name, const int val) { }
void netfile_close_post(struct netfile_post *post) { }
bool netfile_send_post(const char *URL, struct netfile_post *post, FILE *reply_fp,
                       struct netfile_write_cb_data *mem_data, const char *addr) { return FALSE; }
void netfile_free(void) { }
