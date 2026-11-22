# gprolog.mk - GNU Prolog 1.5.0 en SaguiOS (D189). Lo incluye el Makefile.
#
# Dos programas: /bin/gprolog (el top-level) y /bin/pl2wam (el compilador que
# consult/1 lanza para pasar un .pl a byte-code). Los dos se enlazan igual
# que en el GNU Prolog de Linux (gplc): los objetos del motor y de los
# predicados en dos bibliotecas .a, en el orden del upstream, para que los
# módulos que entran y el orden de sus constructores sean los mismos.
#
#   fonte/      el código del upstream (C, y los .pl/.wam de la biblioteca),
#               con los cambios marcados «SaguiOS (D189)» (ver LEEME.txt)
#   gerado/     los .s de i686 de la biblioteca en Prolog y las cabeceras de
#               la configuración; los rehace tools/gprolog_gerar.sh en el
#               anfitrión y se guardan en git
#   (la libc es la común, userland/libc, desde la D210: LIBC_OBJ)
#   plataforma/ la entrada, la pila de C, lanzar procesos y los límites de las
#               pilas de la WAM por software (nuestra)
#
# Las banderas son las del configure de i686 (CFLAGS_MACHINE + CFLAGS):
# -march=pentiumpro (= i686), -fno-strict-aliasing, -fcommon, -O3 con
# -fomit-frame-pointer, -funsigned-char en BipsPl y engine1.c sin optimizar
# (lo pide el upstream). Sin registros de la máquina para la WAM
# (--disable-regs, D189), así que no hace falta -ffixed-ebx. Los reales van
# con la x87, como el GNU Prolog de Linux en i686.

GP_DIR    := userland/terceros/gprolog
GP_F      := $(GP_DIR)/fonte
GP_B      := $(B)/$(GP_DIR)

GP_ENGINE := machine machine1 stacks_sigsegv mem_alloc misc hash_fct hash obj_chain \
             engine engine1 wam_inst atom pred oper if_no_fd main
# OBJLIB de BipsPl/Makefile.in, en su orden: los x.o de un .pl salen de
# gerado/BipsPl-x.s; los x_c.o y x_supp.o, de fonte/BipsPl/x_c.c y x_supp.c.
GP_BIPS_OBJ := $(GP_B)/BipsPl/error_supp.o $(GP_B)/BipsPl/c_supp.o $(GP_B)/BipsPl/foreign_supp.o \
  $(GP_B)/BipsPl/pred_supp.o $(GP_B)/BipsPl/term_supp.o $(GP_B)/BipsPl/stream_supp.o \
  $(GP_B)/BipsPl/scan_supp.o $(GP_B)/BipsPl/parse_supp.o $(GP_B)/BipsPl/write_supp.o \
  $(GP_B)/BipsPl/dynam_supp.o $(GP_B)/BipsPl/callinf_supp.o $(GP_B)/BipsPl/bc_supp.o \
  $(GP_B)/gerado/BipsPl-foreign.o $(GP_B)/gerado/BipsPl-pl_error.o $(GP_B)/gerado/BipsPl-utils.o \
  $(GP_B)/gerado/BipsPl-unify.o $(GP_B)/gerado/BipsPl-assert.o $(GP_B)/BipsPl/assert_c.o \
  $(GP_B)/gerado/BipsPl-read.o $(GP_B)/BipsPl/read_c.o $(GP_B)/gerado/BipsPl-write.o \
  $(GP_B)/BipsPl/write_c.o $(GP_B)/gerado/BipsPl-print.o $(GP_B)/gerado/BipsPl-const_io.o \
  $(GP_B)/BipsPl/const_io_c.o $(GP_B)/gerado/BipsPl-oper.o $(GP_B)/BipsPl/oper_c.o \
  $(GP_B)/gerado/BipsPl-pred.o $(GP_B)/BipsPl/pred_c.o $(GP_B)/gerado/BipsPl-atom.o \
  $(GP_B)/BipsPl/atom_c.o $(GP_B)/gerado/BipsPl-control.o $(GP_B)/BipsPl/control_c.o \
  $(GP_B)/gerado/BipsPl-call.o $(GP_B)/gerado/BipsPl-call_args.o $(GP_B)/BipsPl/call_args_c.o \
  $(GP_B)/gerado/BipsPl-catch.o $(GP_B)/gerado/BipsPl-throw.o $(GP_B)/BipsPl/throw_c.o \
  $(GP_B)/BipsPl/flag_supp.o $(GP_B)/gerado/BipsPl-flag.o $(GP_B)/BipsPl/flag_c.o \
  $(GP_B)/gerado/BipsPl-arith_inl.o $(GP_B)/BipsPl/arith_inl_c.o $(GP_B)/gerado/BipsPl-type_inl.o \
  $(GP_B)/BipsPl/type_inl_c.o $(GP_B)/gerado/BipsPl-term_inl.o $(GP_B)/BipsPl/term_inl_c.o \
  $(GP_B)/gerado/BipsPl-g_var_inl.o $(GP_B)/BipsPl/g_var_inl_c.o $(GP_B)/gerado/BipsPl-all_solut.o \
  $(GP_B)/BipsPl/all_solut_c.o $(GP_B)/gerado/BipsPl-sort.o $(GP_B)/BipsPl/sort_c.o \
  $(GP_B)/gerado/BipsPl-list.o $(GP_B)/BipsPl/list_c.o $(GP_B)/gerado/BipsPl-stat.o \
  $(GP_B)/BipsPl/stat_c.o $(GP_B)/gerado/BipsPl-stream.o $(GP_B)/BipsPl/stream_c.o \
  $(GP_B)/gerado/BipsPl-file.o $(GP_B)/BipsPl/file_c.o $(GP_B)/gerado/BipsPl-char_io.o \
  $(GP_B)/BipsPl/char_io_c.o $(GP_B)/gerado/BipsPl-dec10io.o $(GP_B)/gerado/BipsPl-format.o \
  $(GP_B)/BipsPl/format_c.o $(GP_B)/gerado/BipsPl-os_interf.o $(GP_B)/BipsPl/os_interf_c.o \
  $(GP_B)/gerado/BipsPl-expand.o $(GP_B)/BipsPl/expand_c.o $(GP_B)/gerado/BipsPl-consult.o \
  $(GP_B)/BipsPl/consult_c.o $(GP_B)/gerado/BipsPl-pretty.o $(GP_B)/BipsPl/pretty_c.o \
  $(GP_B)/gerado/BipsPl-random.o $(GP_B)/BipsPl/random_c.o $(GP_B)/gerado/BipsPl-top_level.o \
  $(GP_B)/BipsPl/top_level_c.o $(GP_B)/gerado/BipsPl-debugger.o $(GP_B)/BipsPl/debugger_c.o \
  $(GP_B)/gerado/BipsPl-src_rdr.o $(GP_B)/BipsPl/src_rdr_c.o $(GP_B)/gerado/BipsPl-all_pl_bips.o \
  $(GP_B)/gerado/BipsPl-no_sockets.o $(GP_B)/gerado/BipsPl-no_le_interf.o
GP_ENGINE_OBJ := $(foreach m,$(GP_ENGINE),$(GP_B)/EnginePl/$(m).o)
GP_PL2WAM := pl2wam read_file syn_sugar internal code_gen reg_alloc inst_codif first_arg indexing wam_emit
GP_PL2WAM_OBJ := $(foreach m,$(GP_PL2WAM),$(GP_B)/gerado/Pl2Wam-$(m).o)
GP_PLAT_OBJ := $(GP_B)/plataforma/principal_gp.o $(GP_B)/plataforma/pilhas_gp.o \
               $(GP_B)/plataforma/processos_gp.o
# D210: la libc común (con el printf y el strtod exactos que pide Prolog).
GP_LIBC_OBJ := $(LIBC_OBJ)
GP_LIBS := $(GP_B)/libbips_pl.a $(GP_B)/libengine_pl.a

GP_HDR := $(wildcard $(GP_F)/EnginePl/*.h $(GP_F)/BipsPl/*.h $(GP_DIR)/gerado/EnginePl/*.h \
          $(GP_DIR)/plataforma/*.h) $(GP_F)/EnginePl/wam_archi.def $(LIBC_HDR)

# D464: GNU Prolog es C de antes de C23 (punteros a funcion sin prototipo): GCC 15 usa gnu23 por defecto.
GP_BASE := -m32 -march=pentiumpro -std=gnu17 -fno-strict-aliasing -fcommon \
  -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
  -ffunction-sections -fdata-sections -mfpmath=387 -D__saguios__ \
  -nostdinc -isystem $(shell $(CC) -print-file-name=include) \
  -I$(LIBC_DIR) -I$(GP_DIR)/gerado/EnginePl -I$(GP_F)/EnginePl -I$(GP_F)/BipsPl \
  -I$(GP_F)/TopComp -I$(GP_F)/Linedit -I$(GP_DIR)/plataforma
GP_OPT  := -O3 -fomit-frame-pointer -Wno-char-subscripts
# El código del upstream se compila sin avisos (no es nuestro); lo nuestro, con -Wall.
GP_CF_UP  := $(GP_BASE) $(GP_OPT) -w
GP_CF_NOS := $(GP_BASE) -O2 -Wall -Wextra -Wno-unused-parameter \
  -finput-charset=UTF-8 -fexec-charset=ISO-8859-1 -Iuserland/inc -idirafter kernel/inc
GP_LDFLAGS := -m elf_i386 -nostdlib -T $(GP_DIR)/plataforma/programa_gp.ld --gc-sections \
  -z max-page-size=4096 -z noexecstack --no-warn-rwx-segments

$(GP_B)/EnginePl/engine1.o: $(GP_F)/EnginePl/engine1.c $(GP_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(GP_BASE) -w -c -o $@ $<

$(GP_B)/EnginePl/%.o: $(GP_F)/EnginePl/%.c $(GP_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(GP_CF_UP) -c -o $@ $<

$(GP_B)/BipsPl/%.o: $(GP_F)/BipsPl/%.c $(GP_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(GP_CF_UP) -funsigned-char -c -o $@ $<

$(GP_B)/TopComp/%.o: $(GP_F)/TopComp/%.c $(GP_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(GP_CF_UP) -c -o $@ $<

$(GP_B)/gerado/%.o: $(GP_DIR)/gerado/%.s
	@mkdir -p $(dir $@)
	$(CC) -m32 -c -o $@ $<

$(GP_B)/plataforma/%.o: $(GP_DIR)/plataforma/%.c $(GP_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(GP_CF_NOS) -c -o $@ $<

$(GP_B)/libengine_pl.a: $(GP_ENGINE_OBJ)
	@rm -f $@
	ar rcs $@ $^

$(GP_B)/libbips_pl.a: $(GP_BIPS_OBJ)
	@rm -f $@
	ar rcs $@ $^

# Como gplc: el top-level lleva además top_level.o y debugger.o delante de las
# bibliotecas (y all_pl_bips.o, que hace entrar todos los predicados).
GP_TOP_OBJ := $(GP_B)/TopComp/top_level_main.o $(GP_B)/gerado/BipsPl-all_pl_bips.o \
              $(GP_B)/gerado/BipsPl-top_level.o $(GP_B)/gerado/BipsPl-debugger.o

$(B)/userland/bin/gprolog: $(GP_TOP_OBJ) $(GP_PLAT_OBJ) $(GP_LIBS) $(GP_LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) $(GP_DIR)/plataforma/programa_gp.ld
	@mkdir -p $(dir $@)
	$(LD) $(GP_LDFLAGS) -o $@.elf $(GP_TOP_OBJ) $(GP_PLAT_OBJ) $(GP_LIBS) $(GP_LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

$(B)/userland/bin/pl2wam: $(GP_PL2WAM_OBJ) $(GP_PLAT_OBJ) $(GP_LIBS) $(GP_LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) $(GP_DIR)/plataforma/programa_gp.ld
	@mkdir -p $(dir $@)
	$(LD) $(GP_LDFLAGS) -o $@.elf $(GP_PL2WAM_OBJ) $(GP_B)/gerado/BipsPl-all_pl_bips.o $(GP_PLAT_OBJ) $(GP_LIBS) $(GP_LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf
