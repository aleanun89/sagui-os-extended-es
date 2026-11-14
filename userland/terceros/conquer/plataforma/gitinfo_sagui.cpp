// gitinfo_sagui.cpp - la versión de Vanilla Conquer que se compila (D230).
//
// El upstream genera common/gitinfo.cpp con CMake (GitWatcher) desde el git
// en que se compila. Aquí las fuentes vienen de un tarball fijo
// (tools/conquer_fuentes.sh): los valores salen de su VERSAO, que el
// conquer.mk pasa como definiciones (CNC_COMMIT, CNC_FECHA, CNC_REVISAO).
#include "gitinfo.h"

#ifndef CNC_COMMIT
#define CNC_COMMIT "desconocido"
#endif
#ifndef CNC_FECHA
#define CNC_FECHA ""
#endif
#ifndef CNC_REVISAO
#define CNC_REVISAO 0
#endif

const char GitSHA1[] = CNC_COMMIT;
const char GitShortSHA1[] = {CNC_COMMIT[0], CNC_COMMIT[1], CNC_COMMIT[2], CNC_COMMIT[3], CNC_COMMIT[4], CNC_COMMIT[5], CNC_COMMIT[6], 0};
const char GitCommitDate[] = CNC_FECHA;
const char GitCommitAuthorName[] = "TheAssemblyArmada";
const char GitTag[] = "";
time_t GitCommitTimeStamp = 0;
bool GitUncommittedChanges = false;
bool GitHaveInfo = true;
int GitRevision = CNC_REVISAO;
