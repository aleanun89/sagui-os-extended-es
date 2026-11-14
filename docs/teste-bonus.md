# Guion de prueba: fase bonus (B3 pitido y B4 Sagui 3D)

**Archivos:** `entrega/bonus/saguios.vdi` (VirtualBox) y `entrega/bonus/saguios.img` (pendrive).
Las mismas imágenes están en `entrega/final/`. Cómo grabar: `docs/como-gravar.md`.

B1 (disco duro interno) y B2 (UEFI) los cancelaste tú y aparecen así en el panel.

---

## B3: pitido

| Qué hacer | Qué debe pasar |
| --- | --- |
| En el Terminal: `bip` | Un pitido de 880 Hz durante 0,2 s. |
| `bip 440 1000` | Un la grave durante 1 segundo. |
| `bip escala` | Do-re-mi-fa-sol-la-si-do. |
| Sagui Atrapa Bananas: atrapar una banana / dejar caer una | Pitido agudo corto / dos tonos descendentes. |

**VirtualBox:** el altavoz del PC suele salir por el sonido del ordenador (si no sale, no pasa nada: el PRD ya lo preveía).
**G470:** el PRD avisa de que el portátil puede no conectar esa señal al altavoz. Dime si ha sonado o no, para registrarlo.

---

## B4: Sagui 3D

Ábrelo desde el icono **Sagui 3D** del escritorio (o con `sagui3d` en el Terminal).

**Controles:** las flechas avanzan y giran; **Shift** corre; **Ctrl** o **espacio** dispara con el tirachinas;
**B** suelta una cáscara de banana; **Esc** pausa (en la pausa, **Q** sale); el ratón sobre la vista gira el sagui y el clic dispara.

**El juego:** tres niveles (El claro de piedra, El río seco, El templo del sagui). Junta bananas: la barra de abajo muestra
`actuales/mínimo` de la verja (se pone verde cuando ya se puede salir). Cada banana vale 10 puntos y deja una cáscara;
cinco bananas dan una vida. Los monos capuchinos vienen a quitarte bananas del bolsillo (quitan una vida): tres semillas los dejan
mareados y huyen; una cáscara en el camino hace que el mono resbale y gire. En cada nivel hay un **racimo dorado**
detrás de una pared de lianas que se puede atravesar (pista: busca lianas donde no debería haberlas).

**Qué comprobar:**

| Qué hacer | Qué debe pasar |
| --- | --- |
| Andar por las salas | Paredes con textura de piedra, lianas y madera; salas con cielo, salas oscuras y claras. |
| Subir la escalera del nivel 1 | El sagui sube peldaño a peldaño hasta la plataforma. |
| Llegar a la verja con pocas bananas | `A porteira pede N bananas (faltam M).` y un sonido grave. |
| Disparar a un mono tres veces | Se marea (estrellitas) y luego huye. |
| Comerse una banana y pulsar **B** con un mono acercándose | Resbala, gira y se marea. |
| Juntar el mínimo y volver a la verja | Melodía de fin de nivel y la pantalla de victoria con el racimo dorado "encontrado / no encontrado". |
| Perder las tres vidas | Pantalla de fin de partida. |
| Terminar los tres niveles | Pantalla "¡Libertad!" con los puntos y cuántos racimos dorados has encontrado. |
| Jugar con el pitido | Sonidos distintos para disparo, banana, acierto, vida perdida, cáscara, resbalón y fin de nivel, sin que el juego se bloquee. |

**Fluidez:** en QEMU el juego dibuja de 80 a 110 fotogramas por segundo; en el G470 debería ser más. Si va lento, dímelo.

Cuando lo pruebes, dime **"bonus ok"** o qué ha pasado (las fotos ayudan).
