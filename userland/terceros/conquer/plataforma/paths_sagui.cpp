// paths_sagui.cpp - las rutas de Vanilla Conquer en el SaguiOS (D230).
//
// Sustituye a common/paths_posix.cpp del upstream (que busca el ejecutable en
// /proc/self/exe y guarda en $XDG_CONFIG_HOME/vanilla-conquer), con la misma
// interfaz (common/paths.h, PathsClass) y las rutas del SaguiOS:
//
//   - el programa:  /apps/bin (D194);
//   - los datos:    la carpeta que encontró el lanzador (principal_cnc.c) en
//                   /jogos o en un disco montado, en sagui_cnc_dados; dentro
//                   pueden estar gdi/ y nod/ (los dos discos, D230);
//   - lo del usuario (CONQUER.INI, las partidas, la configuración): /home/cnc.
//
// Código nuestro (GPL-3.0 como el motor, al ir enlazado con él).
#include "paths.h"
#include "debugstring.h"

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

extern "C" {
const char* sagui_cnc_dados = nullptr;   // lo pone el lanzador antes de llamar al main del motor
const char* sagui_cnc_usuario = "/home/cnc";
}

const char* PathsClass::Program_Path()
{
    if (ProgramPath.empty()) {
        ProgramPath = "/apps/bin";
    }
    return ProgramPath.c_str();
}

const char* PathsClass::Data_Path()
{
    if (DataPath.empty()) {
        DataPath = sagui_cnc_dados != nullptr ? sagui_cnc_dados : "/jogos/CnC";
        if (!Suffix.empty() && sagui_cnc_dados == nullptr) {
            DataPath += SEP + Suffix;
        }
    }
    return DataPath.c_str();
}

const char* PathsClass::User_Path()
{
    if (UserPath.empty()) {
        UserPath = sagui_cnc_usuario;
        Create_Directory(UserPath.c_str());
    }
    return UserPath.c_str();
}

bool PathsClass::Create_Directory(const char* dirname)
{
    if (dirname == nullptr) {
        return true;
    }
    std::string temp = dirname;
    size_t pos = 0;
    do {
        pos = temp.find_first_of("/", pos + 1);
        std::string parte = temp.substr(0, pos);
        struct stat st;
        if (stat(parte.c_str(), &st) == 0) {
            continue;
        }
        if (mkdir(parte.c_str(), 0700) != 0 && errno != EEXIST) {
            return false;
        }
    } while (pos != std::string::npos);
    return true;
}

bool PathsClass::Is_Absolute(const char* path)
{
    return path != nullptr && path[0] == '/';
}

std::string PathsClass::Concatenate_Paths(const char* path1, const char* path2)
{
    std::string r(path1);
    if (!r.empty() && r.back() != SEP) {
        r += SEP;
    }
    return r + path2;
}

std::string PathsClass::Get_Filename(const char* path)
{
    const char* p = strrchr(path, '/');
    return std::string(p != nullptr ? p + 1 : path);
}

// La carpeta de argv[0]; en el SaguiOS, la ruta tal cual (no hay enlaces).
std::string PathsClass::Argv_Path(const char* cmd_arg)
{
    std::string r(cmd_arg != nullptr ? cmd_arg : "");
    size_t barra = r.find_last_of('/');
    if (barra == std::string::npos) {
        return std::string();
    }
    return barra == 0 ? std::string("/") : r.substr(0, barra);
}
