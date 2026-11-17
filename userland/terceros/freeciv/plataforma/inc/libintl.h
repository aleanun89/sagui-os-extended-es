/* libintl.h - gettext mínimo para Freeciv en SaguiOS (D374).
 * Lee catálogos .mo estándar (tools/freeciv_mo.py los genera de los .po) de
 * <dir>/<idioma>/LC_MESSAGES/<dominio>.mo. El idioma sale de LANGUAGE o de LANG (el
 * lanzador pone LANG con el idioma del sistema, D313). Implementación: libintl.c. */
#ifndef FC_SAGUI_LIBINTL_H
#define FC_SAGUI_LIBINTL_H
#ifdef __cplusplus
extern "C" {
#endif
char *gettext(const char *msgid);
char *dgettext(const char *dominio, const char *msgid);
char *dcgettext(const char *dominio, const char *msgid, int categoria);
char *ngettext(const char *msgid, const char *plural, unsigned long n);
char *dngettext(const char *dominio, const char *msgid, const char *plural, unsigned long n);
char *textdomain(const char *dominio);
char *bindtextdomain(const char *dominio, const char *dir);
char *bind_textdomain_codeset(const char *dominio, const char *codigo);
#ifdef __cplusplus
}
#endif
#endif
