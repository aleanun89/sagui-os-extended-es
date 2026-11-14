# 027 - HTTPS: TLS 1.2/1.3, certificados y su gestión

- **Estado:** hecha
- **Prioridad:** alta
- **Alta:** 2026-11-07

## Problema / motivación

Con la red (mejora 020) SaguiOS ya descargaba por HTTP, pero casi toda la web solo abre con
`https://`: `curl`, `wget` y el navegador Cipó se quedaban con "https no soportado". Hacía falta
criptografía (sin código de terceros), un generador de números aleatorios de verdad,
certificados X.509 y un almacén de raíces que se pueda gestionar.

## Propuesta (aplicada)

Detalle en `docs/decisoes.md`, D060 a D063.

- **Núcleo** (`kernel/nucleo/aleatorio.c`, D061): reservatorio SHA-256 alimentado por
  RDSEED/RDRAND (si el CPUID los anuncia y funcionan), el TSC de cada interrupción y un "jitter"
  medido en el arranque. Llamadas `SIS_ALEATORIO` (63) y `SIS_INFO_ALEATORIO` (64);
  `/dev/urandom` y `/dev/random`. El ISN de TCP y el ID de DNS pasan a ser imprevisibles.
- **`userland/libtls/`** (D060, D062): SHA-256 (compartido con el núcleo, `kernel/lib/sha256.c`),
  SHA-384/512, HMAC, HKDF, PRF de TLS 1.2, AES-128/256-GCM sin tablas (o con AES-NI si la CPU
  lo tiene), ChaCha20-Poly1305,
  X25519, P-256 y P-384 (ECDH y ECDSA), RSA PKCS#1 v1.5 y PSS (2048-4096 bits); X.509 y PEM;
  cliente TLS 1.3 y TLS 1.2 (ECDHE + AEAD) con SNI, ALPN, HelloRetryRequest, Extended Master
  Secret y protección contra rebaje de versión.
- **Almacén** (D063): 32 raíces públicas en `/etc/ssl/certs/` (copiadas del almacén del anfitrión
  con `tools/raices_ssl.py`); las añadidas van a `/home/.certificados` si `/home` es un disco
  persistente.
- **Programas:** `curl` con `https://`, `-k`/`--inseguro`, `--cacert` y `-v` (versión, suite y
  certificado); `wget` nuevo (barra de progreso, http y https); `certificados` (lista, ver,
  añadir, quitar, probar); `sha256sum`; Cipó abre páginas HTTPS.
- **Pruebas:** `/testes/pruebacripto` (70 comprobaciones con los vectores oficiales y el
  generador) y `tests/h59_tls.py`.

## Criterios de aceptación

- [x] Primitivas comprobadas con los vectores oficiales (FIPS 180-4, FIPS 197, GCM, RFC 4231,
      5869, 7748, 8439, 6979) dentro de SaguiOS
- [x] TLS 1.3 y TLS 1.2, con certificados RSA y ECDSA (cadena con intermedia P-384)
- [x] Errores claros: "emisor desconocido", "nombre no coincide", "certificado caducado"...;
      `-k` los acepta
- [x] `certificados añadir` de una CA y su persistencia en `/home` tras reiniciar
- [x] `wget` de 300 KB por HTTPS con el SHA-256 correcto
- [x] Test automático: `tests/h59_tls.py` (y `h44` adaptado: ya no espera "https no soportado")
- [x] Decisiones D060-D063 en `docs/decisoes.md`, con los límites de seguridad
- [ ] Probado en el G470 (RDRAND no existe en su CPU: entropía solo del TSC y las interrupciones)

## Cifras (QEMU TCG, sin KVM)

- Handshake completo: 20-50 ms del reloj de SaguiOS (resolución 10 ms) contra un servidor local,
  RSA-2048 o ECDSA con intermedia P-384, TLS 1.2 o 1.3.
- AES-256-GCM ~1 MB/s, ChaCha20-Poly1305 ~23 MB/s; `wget` de 300 KB en 0,3 s.
- Imagen: de 1.501.184 a 1.892.864 bytes útiles (+~390 KB: la libtls va enlazada en `curl`, `wget`,
  `certificados` y `cipo`, ~60 KB en cada uno, más 43 KB de raíces).

## Ideas siguientes

- GHASH con PCLMULQDQ (con AES-NI, el GHASH bit a bit es ahora lo más lento del AES-GCM) y
  un AES "troceado" del todo (sin transponer en cada rodada) para las CPU sin AES-NI.
- Revocación: OCSP stapling (el servidor ya lo puede mandar en la extensión `status_request`).
- Reanudación de sesión (tickets de TLS 1.3) para no repetir el handshake en cada petición.
- Ed25519 y P-521 en certificados; nombres internacionalizados (IDN, punycode).
- Una biblioteca compartida o un servicio para no enlazar la libtls en cada programa.
- Guardar entropía en `/home` al apagar y recuperarla al arrancar (semilla persistente).
