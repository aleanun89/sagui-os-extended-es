// SPDX-License-Identifier: GPL-3.0-only
/* arte.c - los dibujos del escritorio, en pixel art hecho en este proyecto.
 *
 * Cada dibujo es una lista de líneas de texto; cada letra es un color de la
 * paleta de abajo y '.' es transparente. Dibujar = recorrer las letras y pintar
 * cuadraditos del tamaño de la escala. */
#include "arte.h"

/* El sagui-de-tufos-blancos: cabeza oscura con la mancha blanca en la frente, los
 * tufos blancos en las orejas, cuerpo gris y la cola rayada. 32x32. */
static const char *const sagui[] = {
    "................................",
    "...........EEEEEEEEEE...........",
    ".........EEEEEEEEEEEEEE.........",
    "..BBB...EEEEEBBBBBEEEEEE..BBB...",
    ".BBBBB.EEEEEEEBBBEEEEEEEEBBBBB..",
    "BBBBBBBEEEEEEEEBEEEEEEEEEBBBBBB.",
    "BBBBBBEEERRRRRRRRRRRRRREEEBBBBB.",
    ".BBBBBEERRPPPRRRRRRPPPRREEBBBBB.",
    "..BBBBEERRPbPRRRRRRPbPRREEBBBB..",
    "...BBBEERRPPPRRRRRRPPPRREEBBB...",
    ".....EEERRRRRRRRRRRRRRRREEE.....",
    "......EERRRRRRPPPRRRRRRREE......",
    "......EEERRRRRRRRRRRRRREEE......",
    ".......EEERRRRPPPPRRRREEE.......",
    "........EEEERRRRRRRREEEE........",
    ".........EEEEEEEEEEEEEE.........",
    "........WWWWWWWWWWWWWWWW........",
    "....wWWWWWCCCCCCCCCCCCWWWWWw....",
    "...wWWMMMCCCCCCCCCCCCCCMMMWWw...",
    "..wWWMMMCCCCCccccccCCCCCMMMWWWw.",
    "..wWWMMCCCCCccccccccCCCCCMMWWw..",
    ".wWWMMMCCCCccccccccccCCCCMMMWWw.",
    ".wWWMMCCCCCccccccccccCCCCCMMWWw.",
    ".wWWMMCCCCCCccccccccCCCCCCMMWWwE",
    ".wWWMMMCCCCCCCCCCCCCCCCCCMMM..EC",
    ".wWWWMMMCCCCCCCCCCCCCCCCMMM..EC.",
    ".wWWWWMMMMCCCCCCCCCCCCMMMM..CE..",
    ".wWWWEEMMMMMMMMMMMMMMMMMMEE.EC..",
    "wwWWEEEE.MMMM......MMMM.EEECE...",
    "...EEEE..EEE........EEE..CECE...",
    "...........................EC...",
    "................................",
};

/* Iconos del escritorio, 32x32. */
static const char *const icone_arquivos[] = {   /* carpeta */
    "................................",
    "................................",
    "................................",
    "................................",
    "..PPPPPPPPPP....................",
    ".PAAAAAAAAAAP...................",
    ".PAAAAAAAAAAAPPPPPPPPPPPPPPPP...",
    ".PAAAAAAAAAAAAAAAAAAAAAAAAAAAP..",
    ".PAAAAAAAAAAAAAAAAAAAAAAAAAAAP..",
    ".PPPPPPPPPPPPPPPPPPPPPPPPPPPPP..",
    ".PYYYYYYYYYYYYYYYYYYYYYYYYYYYP..",
    ".PYYYYYYYYYYYYYYYYYYYYYYYYYYYP..",
    ".PYYYYYYYYYYYYYYYYYYYYYYYYYYYP..",
    ".PYYYYYYYYYYYYYYYYYYYYYYYYYYYP..",
    ".PYYYYYYYYYYYYYYYYYYYYYYYYYYYP..",
    ".PYYYYYYYYYYYYYYYYYYYYYYYYYYYP..",
    ".PYYYYYYYYYYYYYYYYYYYYYYYYYYYP..",
    ".PYYYYYYYYYYYYYYYYYYYYYYYYYYYP..",
    ".PYYYYYYYYYYYYYYYYYYYYYYYYYYYP..",
    ".PYYYYYYYYYYYYYYYYYYYYYYYYYYYP..",
    ".PYYYYYYYYYYYYYYYYYYYYYYYYYYYP..",
    ".PYYYYYYYYYYYYYYYYYYYYYYYYYYYP..",
    ".PYYYYYYYYYYYYYYYYYYYYYYYYYYYP..",
    ".PPPPPPPPPPPPPPPPPPPPPPPPPPPPP..",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
};

static const char *const icone_terminal[] = {   /* monitor con >_ */
    "................................",
    "................................",
    "..PPPPPPPPPPPPPPPPPPPPPPPPPPPP..",
    "..PCCCCCCCCCCCCCCCCCCCCCCCCCCP..",
    "..PCPPPPPPPPPPPPPPPPPPPPPPPPCP..",
    "..PCPPPPPPPPPPPPPPPPPPPPPPPPCP..",
    "..PCPPGGPPPPPPPPPPPPPPPPPPPPCP..",
    "..PCPPPGGPPPPPPPPPPPPPPPPPPPCP..",
    "..PCPPPPGGPPPPPPPPPPPPPPPPPPCP..",
    "..PCPPPPPGGPPPPPPPPPPPPPPPPPCP..",
    "..PCPPPPGGPPPPPPPPPPPPPPPPPPCP..",
    "..PCPPPGGPPPPPPPPPPPPPPPPPPPCP..",
    "..PCPPGGPPPPGGGGGGGPPPPPPPPPCP..",
    "..PCPPPPPPPPPPPPPPPPPPPPPPPPCP..",
    "..PCPPPPPPPPPPPPPPPPPPPPPPPPCP..",
    "..PCPPPPPPPPPPPPPPPPPPPPPPPPCP..",
    "..PCPPPPPPPPPPPPPPPPPPPPPPPPCP..",
    "..PCPPPPPPPPPPPPPPPPPPPPPPPPCP..",
    "..PCCCCCCCCCCCCCCCCCCCCCCCCCCP..",
    "..PPPPPPPPPPPPPPPPPPPPPPPPPPPP..",
    "..............PCCP..............",
    "..............PCCP..............",
    "..........PPPPPPPPPPPP..........",
    "..........PCCCCCCCCCCP..........",
    "..........PPPPPPPPPPPP..........",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
};

static const char *const icone_editor[] = {   /* hoja con líneas y lápiz */
    "................................",
    "......PPPPPPPPPPPPPPPPP.........",
    "......PBBBBBBBBBBBBBBBPP........",
    "......PBBBBBBBBBBBBBBBPBP.......",
    "......PBBBBBBBBBBBBBBBPBBP......",
    "......PBBBBBBBBBBBBBBBPPPPP.....",
    "......PBBCCCCCCCCCCCBBBBBBP.....",
    "......PBBBBBBBBBBBBBBBBBBBP.....",
    "......PBBCCCCCCCCCCCCCCBBBP.....",
    "......PBBBBBBBBBBBBBBBBBBBP.....",
    "......PBBCCCCCCCCCCCCBBBBBP.....",
    "......PBBBBBBBBBBBBBBBBBBBP..PP.",
    "......PBBCCCCCCCCCCCCCCBBBP.PYYP",
    "......PBBBBBBBBBBBBBBBBBBBPPYYP.",
    "......PBBCCCCCCCCCCBBBBBBBPYYP..",
    "......PBBBBBBBBBBBBBBBBBBPYYP...",
    "......PBBCCCCCCCCCCCCCBBPYYP....",
    "......PBBBBBBBBBBBBBBBBPYYP.....",
    "......PBBCCCCCCCCCCCBBPYYP......",
    "......PBBBBBBBBBBBBBBPRRP.......",
    "......PBBBBBBBBBBBBBBPPP........",
    "......PBBBBBBBBBBBBBBBBBP.......",
    "......PBBBBBBBBBBBBBBBBBP.......",
    "......PPPPPPPPPPPPPPPPPPP.......",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
};

static const char *const icone_calculadora[] = {
    "................................",
    ".......PPPPPPPPPPPPPPPPPP.......",
    ".......PCCCCCCCCCCCCCCCCP.......",
    ".......PCGGGGGGGGGGGGGGCP.......",
    ".......PCGGGGGGGGGPPGPGCP.......",
    ".......PCGGGGGGGGGGPPGGCP.......",
    ".......PCGGGGGGGGGGGGGGCP.......",
    ".......PCCCCCCCCCCCCCCCCP.......",
    ".......PCPPPCPPPCPPPCVVCP.......",
    ".......PCPBPCPBPCPBPCVVCP.......",
    ".......PCPPPCPPPCPPPCVVCP.......",
    ".......PCCCCCCCCCCCCCCCCP.......",
    ".......PCPPPCPPPCPPPCVVCP.......",
    ".......PCPBPCPBPCPBPCVVCP.......",
    ".......PCPPPCPPPCPPPCVVCP.......",
    ".......PCCCCCCCCCCCCCCCCP.......",
    ".......PCPPPCPPPCPPPCVVCP.......",
    ".......PCPBPCPBPCPBPCVVCP.......",
    ".......PCPPPCPPPCPPPCVVCP.......",
    ".......PCCCCCCCCCCCCCCCCP.......",
    ".......PCPPPPPPPCPPPCAACP.......",
    ".......PCPBBBBBPCPBPCAACP.......",
    ".......PCPPPPPPPCPPPCAACP.......",
    ".......PCCCCCCCCCCCCCCCCP.......",
    ".......PPPPPPPPPPPPPPPPPP.......",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
};

/* Sagui 3D (B4): pasillo de piedra en perspectiva, cielo arriba, césped
 * abajo y un plátano al fondo. */
static const char *const icone_sagui3d[] = {
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPssssssssssssssssssssssssssssPP",
    "PCPssssssssssssssssssssssssssPCP",
    "PCCPssssssssssssssssssssssssPCCP",
    "PCCkPssssssssssssssssssssssPkCCP",
    "PLCkCPssssssssssssssssssssPCkCLP",
    "PLLkCCPssssssssssssssssssPCCkLLP",
    "PLLLLCkPssssssssssssssssPkCLLLLP",
    "PLLLLLkCPssssssssssssssPCkLLLLLP",
    "PCCLLLLCCPssssssssssssPCCLLLLCCP",
    "PCCkCLLLLkPssssssssssPkLLLLCkCCP",
    "PCCkCCkLLLCPLkLLLkLLPCLLLkCCkCCP",
    "PLCkCCkCCLLCkkkkkkkkCLLCCkCCkCLP",
    "PLLLLLkCCkCLkLLLkLLLLCkCCkLLLLLP",
    "PLLLLLLLLLCCkLLLkLLLCCLLLLLLLLLP",
    "PLLLLLLLLLLLkkkkkkkkLLLLLLLLLLLP",
    "PCCkCCkCCkCCLLLkPLLkCCkCCkCCkCCP",
    "PCCkCCkCCkLLLLLPYLLkLLkCCkCCkCCP",
    "PCCkCCLLLLLCkkPYPkkkCLLLLLCCkCCP",
    "PCLLLLLLLkCLLPYPLLkLLCkLLLLLLLCP",
    "PLLLLLLCCkLPLPPLLLkLPLkCCLLLLLLP",
    "PLLLLCkCCLPggGgggggggPLCCkCLLLLP",
    "PLLkCCkLLPgGggggggggggPLLkCCkLLP",
    "PCCkCCLLPGggggggggggGggPLLCCkCCP",
    "PCCkCLLPggggggggggGgggggPLLCkCCP",
    "PCCLLLPgggggggggGggggggggPLLLCCP",
    "PCLLLPggggggggGggggggggggGPLLLCP",
    "PLLLPgggggggGggggggggggGgggPLLLP",
    "PLLPggggggGggggggggggGggggggPLLP",
    "PLPgggggGggggggggggGgggggggggPLP",
    "PPggggGggggggggggGggggggggggGgPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
};

static const char *const icone_banana[] = {
    "................................",
    "................................",
    "......................PP........",
    ".....................PEEP.......",
    ".....................PEP........",
    "....................PYYP........",
    "...................PYYYYP.......",
    "..................PYYYYYP.......",
    ".................PYYYYYYP.......",
    "................PYYYYYYYP.......",
    "...............PYYYYYYYAP.......",
    "..............PYYYYYYYAP........",
    ".............PYYYYYYYYAP........",
    "...........PPYYYYYYYYAP.........",
    ".........PPYYYYYYYYYAP..........",
    ".......PPYYYYYYYYYYAP...........",
    ".....PPYYYYYYYYYYYAP............",
    "...PPYYYYYYYYYYYAAP.............",
    "..PEYYYYYYYYYYAAPP..............",
    "..PEEYYYYYYAAAPP................",
    "...PPAAAAAAPPP..................",
    ".....PPPPPP.....................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
};

static const char *const icone_sobre[] = {   /* la mascota pequeña: sólo la cabeza */
    "................................",
    "................................",
    "...........EEEEEEEEEE...........",
    ".........EEEEEEEEEEEEEE.........",
    "..BBB...EEEEEBBBBBEEEEEE..BBB...",
    ".BBBBB.EEEEEEEBBBEEEEEEEEBBBBB..",
    "BBBBBBBEEEEEEEEBEEEEEEEEEBBBBBB.",
    "BBBBBBEEERRRRRRRRRRRRRREEEBBBBB.",
    ".BBBBBEERRPPPRRRRRRPPPRREEBBBBB.",
    "..BBBBEERRPbPRRRRRRPbPRREEBBBB..",
    "...BBBEERRPPPRRRRRRPPPRREEBBB...",
    ".....EEERRRRRRRRRRRRRRRREEE.....",
    "......EERRRRRRPPPRRRRRRREE......",
    "......EEERRRRRRRRRRRRRREEE......",
    ".......EEERRRRPPPPRRRREEE.......",
    "........EEEERRRRRRRREEEE........",
    ".......WWEEEEEEEEEEEEEEWW.......",
    "......WWWWWWWWWWWWWWWWWWWW......",
    ".....WWWWWWWWWWWWWWWWWWWWWW.....",
    ".....wwwwwwwwwwwwwwwwwwwwww.....",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
};

static const char *const icone_pendrive[] = {
    "................................",
    "................................",
    "................................",
    "...........PPPPPPPPPP...........",
    "...........PCCCCCCCCP...........",
    "...........PCPPCCPPCP...........",
    "...........PCPPCCPPCP...........",
    "...........PCCCCCCCCP...........",
    ".........PPPPPPPPPPPPPP.........",
    ".........PVVVVVVVVVVVVP.........",
    ".........PVVVVVVVVVVVVP.........",
    ".........PVVVVVVVVVVVVP.........",
    ".........PVVVBBBBBBVVVP.........",
    ".........PVVVBVVVVBVVVP.........",
    ".........PVVVBBBBBBVVVP.........",
    ".........PVVVVVVVVVVVVP.........",
    ".........PVVVVVVVVVVVVP.........",
    ".........PVVVVVVVVVVVVP.........",
    ".........PVVVVVVVVVVVVP.........",
    ".........PVVVVVVVVVVVVP.........",
    ".........PVVVVVVVVVVVVP.........",
    ".........PVVVVVVVVVVVVP.........",
    ".........PVVVVVVVVVVVVP.........",
    "..........PVVVVVVVVVVP..........",
    "...........PPPPPPPPPP...........",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
};

/* Administrador de tareas (D043): una ventana con el gráfico de uso de la CPU. */
static const char *const icone_tarefas[] = {
    "................................",
    "...PPPPPPPPPPPPPPPPPPPPPPPPPP...",
    "...PggggggggggggggggggggBBBgP...",
    "...PggggggggggggggggggggBBBgP...",
    "...PggggggggggggggggggggBBBgP...",
    "...PccccccccccccccccccccccccP...",
    "...PccccccccccccccccccccccccP...",
    "...PckkkkkkkkkkkkkkkkkkkkkkcP...",
    "...PckPPPPkPPPPkPPPPkPPPPkPcP...",
    "...PckPPPPkPPPPkPPPPkPPPPkPcP...",
    "...PckPPPPkPPPPkPPGGkPPPPkPcP...",
    "...PckPPPPkPPGGkPPGGGGPPPkPcP...",
    "...PckkkkkkGGGGkkkGGGGkkkkkcP...",
    "...PckPPPPkGGGGGPPGggGPGGkPcP...",
    "...PckPPPPkGGGgGPGGggGGGGkPcP...",
    "...PckPPPGGGgggGPGggggGGGkPcP...",
    "...PckPPPGGGgggGGGggggggGGPcP...",
    "...PckkkkGgggggGGggggggggGkcP...",
    "...PckPGGGgggggggggggggggGPcP...",
    "...PckPGGGggggggggggggggggPcP...",
    "...PckGGggggggggggggggggggPcP...",
    "...PckggggggggggggggggggggPcP...",
    "...PckggggggggggggggggggggkcP...",
    "...PccccccccccccccccccccccccP...",
    "...PPPPPPPPPPPPPPPPPPPPPPPPPP...",
    ".............CCCCCC.............",
    ".............CCCCCC.............",
    ".........CCCCCCCCCCCCCC.........",
    "................................",
    "................................",
    "................................",
    "................................",
};

/* Visor de imágenes (D055): una foto enmarcada, con montañas y sol. */
static const char *const icone_visor[] = {
    "................................",
    "................................",
    "................................",
    "................................",
    "..PPPPPPPPPPPPPPPPPPPPPPPPPPPP..",
    "..PccccccccccccccccccccccccccP..",
    "..PccccccccccccccccccccccccccP..",
    "..PccPPPPPPPPPPPPPPPPPPPPPPccP..",
    "..PccPssssssssssssssssssssPccP..",
    "..PccPssssssssssssssYYYYssPccP..",
    "..PccPssssssssssssssYYYYssPccP..",
    "..PccPsssssssssssssYYYYYYsPccP..",
    "..PccPssssssssssssssYYYYssPccP..",
    "..PccPssssssssssssssYYYYssPccP..",
    "..PccPsssssBssssssssssssssPccP..",
    "..PccPssssBGBsssssssssssssPccP..",
    "..PccPsssGGgGGsssssGssssssPccP..",
    "..PccPssGGgggGGsssGGGsssssPccP..",
    "..PccPsGGgggggGGsGGgGGssssPccP..",
    "..PccPGGgggggggGGGgggGGsssPccP..",
    "..PccPggggggggggggggggggggPccP..",
    "..PccPggggggggggggggggggggPccP..",
    "..PccPggggggggggggggggggggPccP..",
    "..PccPPPPPPPPPPPPPPPPPPPPPPccP..",
    "..PccccccccccccccccccccccccccP..",
    "..PccccccccccccccccccccccccccP..",
    "..PPPPPPPPPPPPPPPPPPPPPPPPPPPP..",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
};

/* Paint (D056): la paleta de pintor con las tintas y el pincel. */
static const char *const icone_paint[] = {
    "................................",
    "................................",
    "............................ME..",
    "...........................ME...",
    "..........................ME....",
    ".........................ME.....",
    "........................ME......",
    ".......................ME.......",
    "......................ME........",
    "..........MMMMMMMMM..ME.........",
    "........MMAAAAAAAAAMME..........",
    "......MMAAAAAAYYYYAMEMM.........",
    ".....MAArrrrAAYYYYMEAAAM........",
    "....MAAArrrrAAYYYMEAVVVVM.......",
    "...MAAAArrrrAAAACCAAVVVVAM......",
    "...MAAAAAAAAAAACCAAAVVVVAM......",
    "..MAAAAAAAAAAACCAAAAAAAAAAM.....",
    "..MAAAAAAAAAArPAAAAAAAAAAAM.....",
    "..MAAAAAAAAArPAAAAAAAAAAAAM.....",
    "..MAAAAAMAArPAAAAAAAAAAAAAM.....",
    "..MAAAA...AAAAAAAAAGGGGAAAM.....",
    "...MAAM...MAAAAAAAAGGGGAAM......",
    "...MAAA...AAAAAAAAAGGGGAAM......",
    "....MAAAMAAAABBBBAAAAAAAM.......",
    ".....MAAAAAAABBBBAAAAAAM........",
    "......MMAAAAABBBBAAAAMM.........",
    "........MMAAAAAAAAAMM...........",
    "..........MMMMMMMMM.............",
    "................................",
    "................................",
    "................................",
    "................................",
};

/* Configuración (D112): un engranaje gris con el centro azul. */
static const char *const icone_configuracion[] = {
    "................................",
    ".............PPPPPP.............",
    ".............PccccP.............",
    ".......P.....PCCCCP.....P.......",
    "......PcP..PPPCCCCPPP..PcP......",
    ".....PccCPPcccCCCCcccPPCccP.....",
    "....PccCCPcccCCCCCCcccPCCCCP....",
    "...PccCCCCcCCCCCCCCCCcCCCCCCP...",
    "....PCCCCCCCCCCCCCCCCCCCCCCP....",
    ".....PPCCCCCCCCCCCCCCCCCCPP.....",
    ".....PccCCCCCCCCCCCCCCCCCCP.....",
    "....PccCCCCCCPPPPPPCCCCCCCCP....",
    "....PccCCCCCPssssssPCCCCCCCP....",
    ".PPPPcCCCCCPssssssssPCCCCCCPPPP.",
    ".PcCCCCCCCCPssssssssPCCCCCCCCCP.",
    ".PcCCCCCCCCPssssssssPCCCCCCCCCP.",
    ".PcCCCCCCCCPssssssssPCCCCCCCCCP.",
    ".PcCCCCCCCCPssssssssPCCCCCCCCCP.",
    ".PPPPcCCCCCPssssssssPCCCCCCPPPP.",
    "....PccCCCCCPssssssPCCCCCCCP....",
    "....PccCCCCCCPPPPPPCCCCCCCCP....",
    ".....PccCCCCCCCCCCCCCCCCCCP.....",
    ".....PPCCCCCCCCCCCCCCCCCCPP.....",
    "....PCCCCCCCCCCCCCCCCCCCCCCP....",
    "...PccCCCCCCCCCCCCCCCCCCCCCCP...",
    "....PcCCCPCCCCCCCCCCCCPCCCCP....",
    ".....PCCCPPCCCCCCCCCCPPCCCP.....",
    "......PCP..PPPCCCCPPP..PCP......",
    ".......P.....PCCCCP.....P.......",
    ".............PCCCCP.............",
    ".............PPPPPP.............",
    "................................",
};

/* CHIP-8 (D070): una consola de bolsillo con la pantalla verde (el sagui y una
 * banana en píxeles grandes) y el teclado hexadecimal de 4x4. */
static const char *const icone_chip8[] = {
    "................................",
    "................................",
    "..kkkkkkkkkkkkkkkkkkkkkkkkkkkk..",
    ".kCCCCCCCCCCCCCCCCCCCCCCCCCCCCk.",
    ".kCPPPPPPPPPPPPPPPPPPPPPPPPPPCk.",
    ".kCPGPPGPPPPPPPPPPPPPPPPPPPPPCk.",
    ".kCPGGGGPPPPPPPPPPPPPPPPYYPPPCk.",
    ".kCPGPGGPPPPPPPPPPPPPPPYYPPPPCk.",
    ".kCPGGGGPPPPPPPPPPPPPPYYYPPPPCk.",
    ".kCPPGGPPPPPPPPPPPPPPYYYPPPPPCk.",
    ".kCPGGGGPPPPPPPPPPPPPYYPPPPPPCk.",
    ".kCPGPPGPPPPPPPPPPPPPPPPPPPPPCk.",
    ".kCPGGGGGGGGGGGGGGGGGGGGGGGGPCk.",
    ".kCPPPPPPPPPPPPPPPPPPPPPPPPPPCk.",
    ".kCCCCCCCCCCCCCCCCCCCCCCCCCCCCk.",
    ".kCCCCCCCCCCCCCCCCCCCCCCCCCCCCk.",
    ".kCCccCccCccCccCCCCCCCCCCCCCCCk.",
    ".kCCccCccCccCccCCCCCCCCrrrCCCCk.",
    ".kCCCCCCCCCCCCCCCCCCCCrrrrrCCCk.",
    ".kCCccCccCccCccCCCCCCCrrrrrCCCk.",
    ".kCCccCccCccCccCCCCCCCCrrrCCCCk.",
    ".kCCCCCCCCCCCCCCCCCCCCCCCCCCCCk.",
    ".kCCccCccCccCccCCCCggggCCCCCCCk.",
    ".kCCccCccCccCccCCCCggggCCCCCCCk.",
    ".kCCCCCCCCCCCCCCCCCCCCCCCCCCCCk.",
    ".kCCccCccCccCccCCCCCCCCCCCCCCCk.",
    ".kCCccCccCccCccCCCCCCCCCCCCCCCk.",
    ".kCCCCCCCCCCCCCCCCCCCCCCCCCCCCk.",
    "..kkkkkkkkkkkkkkkkkkkkkkkkkkkk..",
    "................................",
    "................................",
    "................................",
};


/* Discos (D121): una unidad gris con la barra de uso en verde. */
static const char *const icone_discos[] = {
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "...PPPPPPPPPPPPPPPPPPPPPPPPPP...",
    "..PccccccccccccccccccccccccccP..",
    "..PccLLLLLLLccccccccccccGGGccP..",
    "..PccLLLLLLLccccccccccccgggccP..",
    "..PccccccccccccccccccccccccccP..",
    "..PkkkkkkkkkkkkkkkkkkkkkkkkkkP..",
    "..PCCCCCCCCCCCCCCCCCCCCCCCCCCP..",
    "..PCkkkkkkkkkkkkkkkkkkkkkkkkCP..",
    "..PCkGGGGGGGGGGGGGBBBBBBBBBkCP..",
    "..PCkGGGGGGGGGGGGGBBBBBBBBBkCP..",
    "..PCkGGGGGGGGGGGGGBBBBBBBBBkCP..",
    "..PCkGGGGGGGGGGGGGBBBBBBBBBkCP..",
    "..PCkGGGGGGGGGGGGGBBBBBBBBBkCP..",
    "..PCkGGGGGGGGGGGGGBBBBBBBBBkCP..",
    "..PCkkkkkkkkkkkkkkkkkkkkkkkkCP..",
    "..PCCkkkkkkkkCCCCCCCCCCCCCCCCP..",
    "..PCCkkkkkkkkCCCCCCCCCCCCCCCCP..",
    "..PCCCCCCCCCCCCCCCCCCCCCCCCCCP..",
    "...PPPPPPPPPPPPPPPPPPPPPPPPPP...",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
};

const desenho_t arte_sagui            = { sagui, 32, 32 };
const desenho_t arte_icone_arquivos   = { icone_arquivos, 32, 32 };
const desenho_t arte_icone_terminal   = { icone_terminal, 32, 32 };
const desenho_t arte_icone_editor     = { icone_editor, 32, 32 };
const desenho_t arte_icone_calculadora = { icone_calculadora, 32, 32 };
const desenho_t arte_icone_banana     = { icone_banana, 32, 32 };
const desenho_t arte_icone_sagui3d    = { icone_sagui3d, 32, 32 };
const desenho_t arte_icone_sobre      = { icone_sobre, 32, 32 };
const desenho_t arte_icone_pendrive   = { icone_pendrive, 32, 32 };
const desenho_t arte_icone_tarefas    = { icone_tarefas, 32, 32 };
const desenho_t arte_icone_visor      = { icone_visor, 32, 32 };
const desenho_t arte_icone_paint      = { icone_paint, 32, 32 };
const desenho_t arte_icone_chip8      = { icone_chip8, 32, 32 };
const desenho_t arte_icone_configuracion = { icone_configuracion, 32, 32 };
const desenho_t arte_icone_discos     = { icone_discos, 32, 32 };

/* ------------------------------------------------------------ iconos de 16x16 (D065)
 * Para el botón Inicio, los botones de las tareas y el menú Inicio. Los programas
 * que ya tienen icono de 32x32 usan el suyo reducido; estos son los que no tienen. */

/* La cabeza del sagui con los tufos blancos: el botón Inicio. */
static const char *const inicio[] = {
    "................",
    ".....EEEEEE.....",
    "...EEEEBBEEEE...",
    ".BB.EEEBBEEE.BB.",
    "BBBEERRRRRREEBBB",
    "BBBERPPRRPPREBBB",
    ".BBERPbRRPbREBB.",
    "..BERRRRRRRREB..",
    "...ERRRPPRRRE...",
    "...EERRRRRREE...",
    "....EERPPREE....",
    ".....EEEEEE.....",
    "....WWWWWWWW....",
    "...wWMCCCCMWw...",
    "...wMCCccCCMw...",
    "....MMMMMMMM....",
};

static const char *const pantalla[] = {   /* un monitor con un paisaje */
    "................",
    ".PPPPPPPPPPPPPP.",
    ".PVVVVVVVVVVVVP.",
    ".PVssssssssssVP.",
    ".PVssssssssssVP.",
    ".PVssssssssGsVP.",
    ".PVsssssssGGGVP.",
    ".PVGGssssGGGGVP.",
    ".PVGGGGGGGGGGVP.",
    ".PVVVVVVVVVVVVP.",
    ".PPPPPPPPPPPPPP.",
    "......PCCP......",
    "......PCCP......",
    "....PPPPPPPP....",
    "....PCCCCCCP....",
    "....PPPPPPPP....",
};

static const char *const cipo[] = {   /* el globo del navegador */
    "................",
    ".....VVVVVV.....",
    "...VVGGVVVVVV...",
    "..VGGGGVVVVGGV..",
    ".VVGGGGGVVVGGVV.",
    ".VVVGGGVVVVGGGV.",
    "VVVVVGGVVVVGGVVV",
    "VVVVVGVVVVVVVVVV",
    "VVVVVVVVVGGGVVVV",
    "VVGGVVVVGGGGGVVV",
    ".VGGGVVVVGGGGVV.",
    ".VVGGGVVVVGGVVV.",
    "..VVGVVVVVVVVV..",
    "...VVVVVVVVVV...",
    ".....VVVVVV.....",
    "................",
};

static const char *const markdown[] = {   /* hoja con una M y la flecha hacia abajo */
    "..PPPPPPPPPP....",
    "..PBBBBBBBBPP...",
    "..PBBBBBBBBPBP..",
    "..PBBBBBBBBPPPP.",
    "..PBBBBBBBBBBBP.",
    "..PBPBBBPBBBBBP.",
    "..PBPPBPPBBBBBP.",
    "..PBPBPBPBBVBBP.",
    "..PBPBBBPBBVBBP.",
    "..PBPBBBPBVVVBP.",
    "..PBPBBBPBBVBBP.",
    "..PBBBBBBBBBBBP.",
    "..PBBBBBBBBBBBP.",
    "..PBBBBBBBBBBBP.",
    "..PPPPPPPPPPPPP.",
    "................",
};

static const char *const infopc[] = {   /* un chip */
    "................",
    "....P.P.P.P.....",
    "....P.P.P.P.....",
    "..PPPPPPPPPPP...",
    "PPPkkkkkkkkkPPP.",
    "..PkkkkkkkkkP...",
    "PPPkkGGGGGkkPPP.",
    "..PkkGkkkGkkP...",
    "PPPkkGkkkGkkPPP.",
    "..PkkGGGGGkkP...",
    "PPPkkkkkkkkkPPP.",
    "..PkkkkkkkkkP...",
    "..PPPPPPPPPPP...",
    "....P.P.P.P.....",
    "....P.P.P.P.....",
    "................",
};

static const char *const mico[] = {   /* Mico (mejora 037): llaves de código en un cuadrado verde */
    "..gggggggggggg..",
    ".gggggggggggggg.",
    "gggggggggggggggg",
    "ggggBBggggBBgggg",
    "gggBggggggggBggg",
    "gggBggggggggBggg",
    "gggBggggggggBggg",
    "ggBggggYYggggBgg",
    "ggBggggYYggggBgg",
    "gggBggggggggBggg",
    "gggBggggggggBggg",
    "gggBggggggggBggg",
    "ggggBBggggBBgggg",
    "gggggggggggggggg",
    ".gggggggggggggg.",
    "..gggggggggggg..",
};

/* ---- Iconos de 16x16 do menu Inicio (D139: os programas sem icone) ---- */

static const char *const recortes[] = {   /* Recortes: uma camara */
    "................",
    "......PPPP......",
    ".....PPPPPP.....",
    "...PPPPPPPPPP...",
    "..PPPPPPPPPPPP..",
    "..PPPPPPPPPPPP..",
    "..PPPPVVVVPPPP..",
    "..PPPVVVVVVPPP..",
    "..PPPVVVVVVPPP..",
    "..PPPVVVVVVPPP..",
    "..PPPPVVVVPPPP..",
    "..PPPPPPPPPPPP..",
    "..PPPPPPPPPPPP..",
    "...PPPPPPPPPP...",
    "................",
    "................",
};

static const char *const icones[] = {   /* Iconos: quatro quadrados coloridos */
    "................",
    "................",
    ".PPPP..PPPP.....",
    ".PYYP..PBBP.....",
    ".PYYP..PBBP.....",
    ".PYYP..PBBP.....",
    ".PPPP..PPPP.....",
    "................",
    "................",
    ".PPPP..PPPP.....",
    ".PGGP..PVVP.....",
    ".PGGP..PVVP.....",
    ".PGGP..PVVP.....",
    ".PPPP..PPPP.....",
    "................",
    "................",
};

static const char *const musica[] = {   /* Musica: uma nota musical */
    "................",
    "...........PPP..",
    "...........PPPP.",
    "...........P.PP.",
    "...........P..P.",
    "...........P....",
    "...........P....",
    "...........P....",
    "...........P....",
    "...........P....",
    "...........P....",
    "........PPPP....",
    ".......PPPPPP...",
    ".......PPPPPP...",
    "........PPPP....",
    "................",
};

static const char *const relogio[] = {   /* Fecha y hora: um relogio */
    "................",
    "....PPPPPPPP....",
    "..PP........PP..",
    ".P............P.",
    ".P......P.....P.",
    ".P......P.....P.",
    ".P......P.....P.",
    ".P......P.....P.",
    ".P....PPP.....P.",
    ".P...PP.......P.",
    ".P...P........P.",
    ".P............P.",
    ".P............P.",
    "..PP........PP..",
    "....PPPPPPPP....",
    "................",
};

static const char *const visorpdf[] = {   /* Visor de PDF: uma folha com um A vermelho */
    "................",
    "....PPPPPPPPP...",
    "....PBBBBBBP...",
    "....PBBBBBBP...",
    "....PBBBBBBP...",
    "....P..WWW..P...",
    "....P.W...W.P...",
    "....P.W...W.P...",
    "....PWW...WWP...",
    "....PW.....WP...",
    "....PW.WWW.WP...",
    "....PW.....WP...",
    "....PBBBBBBP...",
    "....PPPPPPPPP...",
    "................",
    "................",
};

static const char *const video[] = {   /* Reprodutor de video: um ecra com o botao de play */
    "................",
    "..PPPPPPPPPPPP..",
    "..PVVVVVVVVVVV..",
    "..PVVVVBVVVVV..",
    "..PVVVBBVVVVV..",
    "..PVVVBBBVVVV..",
    "..PVVVBBBBVVV..",
    "..PVVVBBBBVVV..",
    "..PVVVBBBVVVV..",
    "..PVVVBBVVVVV..",
    "..PVVVVBVVVVV..",
    "..PVVVVVVVVVVV..",
    "..PPPPPPPPPPPP..",
    "................",
    "................",
    "................",
};

static const char *const macaco[] = {   /* Macaco: uma folha de calculo com a barra de titulo verde */
    "................",
    ".PPPPPPPPPPPPPP.",
    ".PggggggggggggP.",
    ".PPPPPPPPPPPPPP.",
    ".PBBBBBBBBBBBBP.",
    ".PBBPBBPBBPBBPP.",
    ".PPPPPPPPPPPPPP.",
    ".PggPBBPBBPBBPP.",
    ".PBBPBBPBBPBBPP.",
    ".PPPPPPPPPPPPPP.",
    ".PBBPBBPBBPYYPP.",
    ".PBBPBBPBBPBBPP.",
    ".PBBBBBBBBBBBBP.",
    ".PPPPPPPPPPPPPP.",
    "................",
    "................",
};

static const char *const pluma[] = {   /* Pluma: una hoja con renglones y una pluma azul (D291) */
    "................",
    "..PPPPPPPPP.....",
    "..PBBBBBBBPP....",
    "..PBPPPPPBPBP...",
    "..PBBBBBBBPPPP..",
    "..PBPPPPPPBBBP..",
    "..PBBBBBBBBBBP..",
    "..PBPPPPPPPPBP..",
    "..PBBBBBBBBBBP..",
    "..PBPPPPPBBVVP..",
    "..PBBBBBBBVVVP..",
    "..PBPPPPPVVVVPP.",
    "..PBBBBBVVVPPVP.",
    "..PPPPPPVVPP.PP.",
    ".........PP.....",
    "................",
};

static const char *const ia[] = {   /* IA: uma cabeca de robo */
    "................",
    ".......P........",
    ".......g........",
    "...gggggggggg...",
    "..gggggggggggg..",
    "..ggBBggggBBgg..",
    "..ggBBggggBBgg..",
    "..gggggggggggg..",
    "..gggggggggggg..",
    "..gggggggggggg..",
    "..gggggggggggg..",
    "..gggggggggggg..",
    "...gggggggggg...",
    "....gggggggg....",
    "................",
    "................",
};

static const char *const chatia[] = {   /* Chat IA: uma bolha de fala */
    "................",
    "................",
    "..PPPPPPPPPPPP..",
    ".PVVVVVVVVVVVVP.",
    ".PVBBVVVVVVBBVP.",
    ".PVVVVVVVVVVVVP.",
    ".PVVVVVVVVVVVVP.",
    ".PVVVVVVVVVVVVP.",
    ".PVVVVVVVVVVVVP.",
    "..PPPPPPPPPPPP..",
    "..PP............",
    "...PP...........",
    "....P...........",
    "................",
    "................",
    "................",
};

static const char *const correu[] = {   /* Correo: um envelope */
    "................",
    "................",
    "..PPPPPPPPPPPP..",
    ".PBBBBBBBBBBBBP.",
    ".PBBBBBBBBBBBBP.",
    ".PVBBBBBBBBBBVP.",
    ".PVBVVVVVVVVBVP.",
    ".PVBVBBBBBBVBVP.",
    ".PVBVBVBBVBVBVP.",
    ".PBBBBBBBBBBBBP.",
    ".PBBBBBBBBBBBBP.",
    ".PBBBBBBBBBBBBP.",
    "..PPPPPPPPPPPP..",
    "................",
    "................",
    "................",
};

static const char *const spider[] = {   /* Spider: dos cartas (pica y corazón) sobre el tapete verde (D173) */
    "................",
    ".PPPPPPPPPPPPPP.",
    ".PggggggggggggP.",
    ".PgPPPPPPPPPPPP.",
    ".PgPBBBBPBBBBPP.",
    ".PgPBPPBPrBBrPP.",
    ".PgPPPPPPrrrrPP.",
    ".PgPPPPPPrrrrPP.",
    ".PgPBPPBPBrrBPP.",
    ".PgPBPPBPBBBBPP.",
    ".PgPPPPPPBBBBPP.",
    ".PgPBBBBPBBBBPP.",
    ".PgPPPPPPPPPPPP.",
    ".PggggggggggggP.",
    ".PPPPPPPPPPPPPP.",
    "................",
};

static const char *const buscaminas[] = {   /* Buscaminas: rejilla verde con una mina y una bandera (D248) */
    "................",
    ".PPPPPPPPPPPPPP.",
    ".PggggPggggPggP.",
    ".PggggPggggPggP.",
    ".PggggPgrrgPggP.",
    ".PPPPPPPrrPPPPP.",
    ".PggPBBBBBBPggP.",
    ".PggPBBPPBBPggP.",
    ".PPPPBPPPPBPPPP.",
    ".PggPBBPPBBPggP.",
    ".PggPBBBBBBPggP.",
    ".PPPPPPPPPPPPPP.",
    ".PggggPggggPggP.",
    ".PggggPggggPggP.",
    ".PPPPPPPPPPPPPP.",
    "................",
};

static const char *const jclic[] = {   /* JClic: una pieza de puzle azul con una pestaña amarilla sobre un cuaderno (D185) */
    "................",
    "......PPPP......",
    ".....PYYYYP.....",
    ".....PYYYYP.....",
    ".PPPPPPYYPPPPPP.",
    ".PVVVVVVVVVVVVP.",
    ".PVsssssssssVVP.",
    ".PVVVVVVVVVVVVP.",
    ".PVVVVVVVVVVVVP.",
    ".PVVVVVVVVVVVVP.",
    ".PVVVVVVVVVVVVP.",
    ".PPPPPVVVVPPPPP.",
    ".....PVVVVP.....",
    ".....PVVVVP.....",
    ".....PPPPPP.....",
    "................",
};

static const char *const jclicbajar[] = {   /* la misma pieza con una flecha verde hacia abajo (D185) */
    "................",
    "......PPPP......",
    ".....PGGGGP.....",
    ".....PGGGGP.....",
    ".PPPPPPGGPPPPPP.",
    ".PVVVVVGGVVVVVP.",
    ".PVVVVVGGVVVVVP.",
    ".PVVGGGGGGGGVVP.",
    ".PVVVGGGGGGVVVP.",
    ".PVVVVGGGGVVVVP.",
    ".PVVVVVGGVVVVVP.",
    ".PPPPPVVVVPPPPP.",
    ".....PVVVVP.....",
    ".....PVVVVP.....",
    ".....PPPPPP.....",
    "................",
};

#ifndef SAGUI_SEM_GPL   /* D180: el OpenTyrian no va en la variante sin GPL */
static const char *const tyrian[] = {   /* OpenTyrian: una nave blanca y gris sobre el espacio (D180) */
    "PPPPPPPPPPPPPPPP",
    "PkPPPPPPPPPPkPPP",
    "PPPPPPPBPPPPPPPP",
    "PPPPPPPBBPPPPPkP",
    "PPPPPPcBBcPPPPPP",
    "PPPPPPcBBcPPPPPP",
    "PPPPPCcBBcCPPPPP",
    "PkPPPCcVVcCPPPPP",
    "PPPPCCcVVcCCPPPP",
    "PPPCCCcBBcCCCPPP",
    "PPrCCPcBBcPCCrPP",
    "PPrCPPCBBCPPCrPP",
    "PPPPPPPYYPPPPkPP",
    "PPPPPPPYYPPPPPPP",
    "PPkPPPPAAPPPPPPP",
    "PPPPPPPPPPPPPPPP",
};
#endif

#ifndef SAGUI_SEM_GPL   /* D228: OpenJazz es GPL */
static const char *const openjazz[] = {   /* OpenJazz: Jazz, el conejo verde con la cinta roja y el arma azul (D228) */
    "....PP....PP....",
    "...PGGP..PGGP...",
    "...PGGP..PGGP...",
    "...PGGP..PGGP...",
    "....PGGPPGGP....",
    "....PGGGGGGP....",
    "...PGGGGGGGGP...",
    "...PGBPGGBPGP...",
    "...PrrrrrrrrP...",
    "....PGGGGGGP....",
    ".....PGGGGP.....",
    "....PGGGGGGPVVV.",
    "...PGGPGGPGPVVP.",
    "...PGP.PGGP.PP..",
    "...PP..PGGP.....",
    ".......PPPP.....",
};
#endif

#ifndef SAGUI_SEM_GPL   /* D230: Command & Conquer (Vanilla Conquer) es GPL */
static const char *const conquer[] = {   /* Command & Conquer: un tanque verde junto a cristales de tiberio (D230) */
    "................",
    "................",
    "......PPPP......",
    ".....PGGGGP.....",
    ".....PGGGGPPPPPP",
    ".....PGGGGP.....",
    "...PPPPPPPPPP...",
    "..PGGGGGGGGGGP..",
    ".PGGGGGGGGGGGGP.",
    ".PPPPPPPPPPPPPP.",
    ".PCkCkCkCkCkCkP.",
    "..PCkCkCkCkCkP..",
    "...PPPPPPPPPP...",
    "..g..........g..",
    ".ggg.g....g.ggg.",
    "gggggggg.gggggg.",
};
#endif

#ifndef SAGUI_SEM_GPL   /* D229: Quake es GPL */
static const char *const quake[] = {   /* Quake: el emblema, el anillo con el clavo (D229) */
    "...PPP....PPP...",
    "..PMMP....PMMP..",
    ".PMMP..PP..PMMP.",
    "PMMP..PMMP..PMMP",
    "PMEP..PMMP..PEMP",
    "PMEP..PMMP..PEMP",
    "PMEP..PMMP..PEMP",
    "PMMP..PMMP..PMMP",
    ".PMMP.PMMP.PMMP.",
    "..PMMPPMMPPMMP..",
    "...PPMMMMMMPP...",
    ".....PPMMPP.....",
    "......PMMP......",
    "......PEEP......",
    ".......PP.......",
    "................",
};
#endif

#ifndef SAGUI_SEM_GPL   /* D235: Game Boy (SDL2-GNUBoy) es GPL */
static const char *const gnuboy[] = {   /* Game Boy: la consola gris con la pantalla verde, la cruceta y los botones (D235) */
    "...PPPPPPPPPP...",
    "..PCCCCCCCCCCP..",
    "..PCPPPPPPPPCP..",
    "..PCPgggggGPCP..",
    "..PCPgGGGGGPCP..",
    "..PCPgGGGGGPCP..",
    "..PCPPPPPPPPCP..",
    "..PCCCCCCCCCCP..",
    "..PCCPCCCCCrCP..",
    "..PCPPPCCCrrrP..",
    "..PCCPCCCCCrrP..",
    "..PCCCCCCCCCCP..",
    "..PCCCCPPCPPCP..",
    "..PCCCCCCCCCCPP.",
    "..PCCCCCCCCCCCP.",
    "...PPPPPPPPPPP..",
};
#endif

#ifndef SAGUI_SEM_GPL   /* D184: NetSurf es GPL */
static const char *const netsurf[] = {   /* NetSurf: un globo azul con la ola blanca (D184) */
    "......PPPP......",
    "....PPVVVVPP....",
    "...PVVsGGVVVP...",
    "..PVVsGGGGVVVP..",
    ".PVVVGGGGGVVVVP.",
    ".PVBBBVGGVVVVVP.",
    "PVBBBBBBVVVBBVVP",
    "PBBVVVBBBBBBBBVP",
    "PVVVVVVBBBBVVVVP",
    "PVVVsVVVVVVVVVVP",
    ".PVVGGVVVVVGGVP.",
    ".PVVGGGVVVGGGVP.",
    "..PVVGGVVVGGVP..",
    "...PVVVVVVVVP...",
    "....PPVVVVPP....",
    "......PPPP......",
};
#endif

static const char *const bluetooth[] = {   /* Bluetooth: círculo azul con la runa blanca (D279) */
    "....PPPPPPPP....",
    "..PPVVVVVVVVPP..",
    ".PVVVVVBVVVVVVP.",
    ".PVVVVVBBVVVVVP.",
    "PVVVVVVBVBVVVVVP",
    "PVBVVVVBVVBVVVVP",
    "PVVBBVVBVBVVVVVP",
    "PVVVVBBBBVVVVVVP",
    "PVVVVBBBBVVVVVVP",
    "PVVBBVVBVBVVVVVP",
    "PVBVVVVBVVBVVVVP",
    "PVVVVVVBVBVVVVVP",
    ".PVVVVVBBVVVVVP.",
    ".PVVVVVBVVVVVVP.",
    "..PPVVVVVVVVPP..",
    "....PPPPPPPP....",
};

const desenho_t arte_inicio           = { inicio, 16, 16 };
const desenho_t arte_mini_bluetooth   = { bluetooth, 16, 16 };   /* D279 */
const desenho_t arte_mini_mico        = { mico, 16, 16 };
const desenho_t arte_mini_pantalla    = { pantalla, 16, 16 };
const desenho_t arte_mini_cipo        = { cipo, 16, 16 };
const desenho_t arte_mini_markdown    = { markdown, 16, 16 };
const desenho_t arte_mini_infopc      = { infopc, 16, 16 };
const desenho_t arte_mini_recortes    = { recortes, 16, 16 };
const desenho_t arte_mini_icones      = { icones, 16, 16 };
const desenho_t arte_mini_musica      = { musica, 16, 16 };
const desenho_t arte_mini_relogio     = { relogio, 16, 16 };
const desenho_t arte_mini_visorpdf    = { visorpdf, 16, 16 };
const desenho_t arte_mini_video       = { video, 16, 16 };
const desenho_t arte_mini_ia          = { ia, 16, 16 };
const desenho_t arte_mini_macaco      = { macaco, 16, 16 };
const desenho_t arte_mini_pluma       = { pluma, 16, 16 };   /* mejora 116, D291 */
const desenho_t arte_mini_spider      = { spider, 16, 16 };   /* D173: el solitario Spider */
const desenho_t arte_mini_buscaminas  = { buscaminas, 16, 16 };   /* D248: el Buscaminas */
const desenho_t arte_mini_jclic       = { jclic, 16, 16 };   /* D185: actividades JClic */
const desenho_t arte_mini_jclicbajar  = { jclicbajar, 16, 16 };   /* D185: bajar actividades JClic */
#ifndef SAGUI_SEM_GPL
const desenho_t arte_mini_tyrian      = { tyrian, 16, 16 };   /* D180: o OpenTyrian */
const desenho_t arte_mini_netsurf     = { netsurf, 16, 16 };  /* D184: el navegador NetSurf */
const desenho_t arte_mini_openjazz    = { openjazz, 16, 16 }; /* D228: OpenJazz */
const desenho_t arte_mini_conquer     = { conquer, 16, 16 };  /* D230: Command & Conquer */
const desenho_t arte_mini_quake       = { quake, 16, 16 };    /* D229: Quake */
const desenho_t arte_mini_gnuboy      = { gnuboy, 16, 16 };   /* D235: Game Boy (SDL2-GNUBoy) */
#endif
const desenho_t arte_mini_chatia      = { chatia, 16, 16 };
const desenho_t arte_mini_correo      = { correu, 16, 16 };   /* 082: o cliente de correio */

/* Paleta: letra -> color. Devuelve false para transparente. */
bool arte_cor(char letra, uint32_t *cor)
{
    switch (letra) {
    case 'P': *cor = 0x141210; return true;   /* negro (contorno) */
    case 'E': *cor = 0x3C2D23; return true;   /* marrón oscuro (cabeza, cola) */
    case 'M': *cor = 0x6E553C; return true;   /* marrón */
    case 'C': *cor = 0x968C7D; return true;   /* gris */
    case 'c': *cor = 0xBEB4A5; return true;   /* gris claro (pecho) */
    case 'B': *cor = 0xF5F5F0; return true;   /* blanco (tufos, frente) */
    case 'R': *cor = 0xAA7864; return true;   /* rosado (cara) */
    case 'b': *cor = 0xFFFFFF; return true;   /* brillo en el ojo */
    case 'Y': *cor = 0xFFD52E; return true;   /* amarillo */
    case 'A': *cor = 0xD9A400; return true;   /* amarillo oscuro */
    case 'G': *cor = 0x3CDC6E; return true;   /* verde */
    case 'V': *cor = 0x2A6FD8; return true;   /* azul */
    case 'k': *cor = 0x5A564E; return true;   /* piedra oscura (Sagui 3D) */
    case 'L': *cor = 0x8F8A7C; return true;   /* piedra clara */
    case 's': *cor = 0x7EC8E3; return true;   /* cielo */
    case 'g': *cor = 0x0B7A3E; return true;   /* verde del SaguiOS */
    case 'r': *cor = 0xD83A30; return true;   /* rojo (tinta del Paint) */
    case 'W': *cor = 0xC8202A; return true;   /* rojo liso de la capa del sagui (D101) */
    case 'w': *cor = 0x7A1218; return true;   /* sombra de la capa */
    default: return false;
    }
}
