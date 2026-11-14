# RFC 7932 (Brotli): diccionario, transformaciones y tablas de contexto (D262)

- **Datos:** `userland/web/brotli_tab.h`: diccionario estático (Apéndice A de la RFC, 122 784 bytes),
  transformaciones (Apéndice B) y tablas de contexto (sección 7.1), generados de `rfc7932.txt`. Los
  datos no se han movido de sitio.
- **Origen y licencia:** RFC 7932 (Alakuijala y Szabadka, Google; © IETF Trust). La RFC dice que su
  texto de código se puede usar bajo la licencia BSD revisada de las «Trust Legal Provisions»; el
  diccionario de Brotli de Google es además MIT (<https://github.com/google/brotli>). Los dos son
  compatibles con la GPL v3. Se trata como **datos de una norma**, con crédito a la RFC.
- El descompresor (`brotli.c`) es del motor de robson-apk (GPL-3.0), escrito a partir de la RFC.
