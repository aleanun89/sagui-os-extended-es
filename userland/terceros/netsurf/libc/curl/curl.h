// SPDX-License-Identifier: GPL-2.0-or-later
/* curl/curl.h - solo el tipo CURLM (D184).
 *
 * content/fetch.c del upstream incluye content/fetchers/curl.h aunque se
 * compile sin libcurl (WITH_CURL apagado), y esa cabecera declara un
 * "extern CURLM *fetch_curl_multi". El SaguiOS no tiene libcurl: la red de
 * NetSurf va por un fetcher propio sobre la HTTP de la libsagui
 * (plataforma/fetch_http.c). Ver docs/carencias-netsurf.md. */
#ifndef NS_CURL_CURL_H
#define NS_CURL_CURL_H
typedef struct ns_curl_multi CURLM;
#endif
