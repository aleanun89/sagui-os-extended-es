# libcxx.mk - el runtime mínimo de C++ del SaguiOS (D228). Lo incluye el Makefile.
#
# Para los programas de C++ portados (OpenJazz, Vanilla Conquer): sin la biblioteca estándar
# de C++ del anfitrión (-nostdinc++), con las cabeceras finas de
# userland/libcxx/include sobre la libc común (D210), excepciones con nuestro
# desenrollador (rt/desenrolar.c) y sin RTTI. Quien enlaza C++ usa
# userland/programa_cxx.ld (conserva .eh_frame) y añade $(CXXRT_OBJ).
#
# Las cabeceras (userland/libcxx/include), lo que los ports han ido pidiendo:
#   <new> <memory> (unique_ptr) <utility> <type_traits> <initializer_list>
#   <vector> <list> <algorithm> (D228, OpenJazz);
#   <string> (std::string, to_string, sto*), <chrono> (duration, time_point,
#   steady_clock/system_clock sobre clock_gettime), <ratio>, <exception> y
#   <stdexcept> (D230, Vanilla Conquer);
#   <cstdlib> <cmath> <cstring> <cstdio> <cctype> <ctime> <cstdint> <cstddef>
#   <cstdarg> <cassert> <cerrno> <climits>: las de C, con sus nombres también
#   en std:: (std::abs con todas sus sobrecargas, std::pow, std::memcpy...).
# Lo que no hay (iostream, map, set, unordered_*, thread, regex...) se añade
# cuando lo pida un port, general y probado en tests/cxx_biblioteca_unidad.cpp.
#
# El compilador: $(CXX) (g++). El entorno local (tools/entorno-local.sh) trae
# el cc1plus de g++-13 y un envoltorio g++ como el de gcc. Si no hay
# compilador de C++, CXX_OK queda vacío y lo que es C++ no se compila (el
# sistema se construye igual, sin esos programas).

CXX_OK := $(shell $(CXX) -m32 -x c++ -fsyntax-only /dev/null >/dev/null 2>&1 && echo 1)
ifneq ($(CXX_OK),1)
$(info libcxx.mk: no hay compilador de C++ ($(CXX)); los programas de C++ (OpenJazz) no se compilan. Ejecute tools/entorno-local.sh.)
endif

CXXRT_DIR    := userland/libcxx
CXXRT_ENVOLT := $(B)/cxx/envoltorios
CXXRT_HDR    := $(wildcard $(CXXRT_DIR)/include/*) $(wildcard $(CXXRT_DIR)/rt/*.h)

# Las de todo el C++ del sistema. Los __linux__ y compañía los define el gcc
# del anfitrión, pero el programa no corre en Linux: fuera, para que el código
# portado no busque cosas de Linux (rutas XDG, terminal...).
CXXFLAGS_SAGUI := -m32 -march=i686 -std=gnu++17 -Os \
  -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie -fno-stack-protector \
  -fexceptions -fno-rtti -ffunction-sections -fdata-sections \
  -mfpmath=387 -fexcess-precision=standard \
  -nostdinc -nostdinc++ -isystem $(shell $(CC) -print-file-name=include) \
  -I$(CXXRT_ENVOLT) -I$(CXXRT_DIR)/include -I$(LIBC_DIR) \
  -U__linux__ -U__linux -Ulinux -U__gnu_linux__ -U__unix__ -U__unix -Uunix -D__SAGUIOS__

ULDFLAGS_CXX := $(subst userland/programa.ld,userland/programa_cxx.ld,$(ULDFLAGS))

CXXRT_OBJ := $(addprefix $(B)/$(CXXRT_DIR)/rt/,desenrolar.o abi_tipos.o cxxrt_sistema.o excecoes.o abi.o)

# Las cabeceras de la libc con «extern "C"» (tools/cxx_envoltorios.py).
$(CXXRT_ENVOLT)/.feito: $(LIBC_HDR) tools/cxx_envoltorios.py
	@mkdir -p $(CXXRT_ENVOLT)
	$(PY) tools/cxx_envoltorios.py $(LIBC_DIR) $(CXXRT_ENVOLT)
	@touch $@

# La parte en C: el desenrollador y los typeinfo no usan la libc; el aviso, sí.
$(B)/$(CXXRT_DIR)/rt/%.o: $(CXXRT_DIR)/rt/%.c $(CXXRT_HDR) $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(LIBC_CFLAGS) -Wall -Wextra -Wno-unused-parameter -I$(CXXRT_DIR)/rt \
	  -Iuserland/inc -idirafter kernel/inc -c -o $@ $<

# La parte en C++ (con tablas de desenrollado: __cxa_throw es el primer cuadro que se sube).
$(B)/$(CXXRT_DIR)/rt/%.o: $(CXXRT_DIR)/rt/%.cpp $(CXXRT_HDR) $(CXXRT_ENVOLT)/.feito
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_SAGUI) -Wall -Wextra -I$(CXXRT_DIR)/rt -c -o $@ $<
