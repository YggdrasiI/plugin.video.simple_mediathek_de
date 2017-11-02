############################################################

SCRIPT_NAME=simple_mediathek
PROG_NAME=$(SCRIPT_NAME).bin
BROTLI=1

# Scripts will installed in $INSTALL_PREFIX/bin
# Binaries will insatlled in $INSTALL_PREFIX/$MACH
INSTALL_PREFIX=../root
MACH=$(shell uname -m)
GIT_COMMIT_VERSION=$(shell git log --oneline --max-count=1 | head --bytes=7)
VERSION='MediathekView data parser 0.7 ($(GIT_COMMIT_VERSION))'
BROTLI_STATIC=1

############################################################

#CC=gcc-4.9
CFLAGS=-std=c11 -Wpedantic -Wno-implicit-function-declaration
#CFLAGS+= static
ODIR=obj

ifeq ($(MACH),armv7l)
	COPY_TO_ARMV6L=copy_to_armv6l copy_lib_to_armv6l
endif

ifeq ($(MAKECMDGOALS),rpi)
CFLAGS_RPI=-Ofast -mfpu=vfp -mfloat-abi=hard -march=armv6zk -mtune=arm1176jzf-s
CFLAGS=$(CFLAGS_RPI)
ODIR=obj_rpi
#MACH=armv6l
endif

ifeq ($(MAKECMDGOALS),debug)
CFLAGS+= -g
ODIR=obj_debug
BROTLI=0
else
CFLAGS+= -O3 -DNDEBUG
CFLAGS+= -Wall
endif

# Bugfix (use debug variant for 'make install'
ifeq ($(MAKECMDGOALS),install)
CFLAGS+= -g
ODIR=obj_debug
#PROG_NAME=$(PROG_NAME)_debug
endif

CFLAGS+= -DVERSION=\"$(VERSION)\"

############################################################

FILES=main.c helper.c hashmap.c utf8.c \
			parser.c filmliste.c channels.c linked_list.c \
			params.c search.c payload.c info.c \
			search_output.c search_sortings.c

DEPS=settings.h
#DEPS=$$(ls *.h)

ifneq ($(BROTLI),0)
# With Brotli
BROTLI_SRC=brotli/release_$(MACH)
BROTLI_ROOT=${INSTALL_PREFIX}/${MACH}
ifneq ($(BROTLI_STATIC),0)
# With static Brotli
BROTLI_DEP=libbrotli_static
BROTLI_LIBS_STATIC=$(BROTLI_SRC)/libbrotlidec-static.a $(BROTLI_SRC)/libbrotlienc-static.a $(BROTLI_SRC)/libbrotlicommon-static.a -lm
CFLAGS+=-DCOMPRESS_BROTLI -I$(BROTLI_SRC)/../c/include -lm
else
# dynamic linked Brotli
BROTLI_DEP=libbrotli_dynamic
BROTLI_LIBS=-lbrotlidec -lbrotlienc -lbrotlicommon
BROTLI_LIBS_STATIC=
CFLAGS+=-DCOMPRESS_BROTLI -I$(BROTLI_ROOT)/include  -L$(BROTLI_ROOT)/lib $(BROTLI_LIBS)
endif

#CFLAGS+=-DCOMPRESS_BROTLI $$(PKG_CONFIG_PATH=$(BROTLI_ROOT)/lib/pkgconfig/ pkg-config --libs --cflags brotli)
FILES+=brotli.c
else
# Without Brotli, set dependency on existing file to omit brotli compiling
BROTLI_DEP=Makefile
endif

#_OBJ = $(patsubst %.c, %.o, $(FILES))
OBJ = $(FILES:%.c=$(ODIR)/%.o)

## Variables for testing targets
TEST_PREFIX=/dev/shm/
TEST_OUTPUT=$(TEST_PREFIX)$(SCRIPT_NAME).out
# Test command chain should be
# $(TEST_BEFORE_CMD) you_commands $(TEST_AFTER_CMD)
TEST_BEFORE_CMD=(test -f $(TEST_PREFIX).$@ && echo "Test '$@' skipped") \
								|| (echo "Start '$@'" &&
TEST_AFTER_CMD= >"$(TEST_OUTPUT)" 2>&1 \
								&& touch "$(TEST_PREFIX).$@" && echo "Test '$@' success") \
								|| (echo "Test '$@' failed\tSee $(TEST_OUTPUT) for output" && exit 1)

## Variables for local hosting of input files
DATA_DIR=../data
FILMLISTE_AKT_XZ=$(DATA_DIR)/Filmliste-akt.xz
FILMLISTE_DIFF_XZ=$(DATA_DIR)/Filmliste-diff.xz

## Variables for indexing of local file
FILMLISTE_AKT_JSON=/dev/shm/Filmliste-akt.json
############################################################

main: $(BROTLI_DEP) $(ODIR) $(PROG_NAME)
	@echo "Done"

debug: main
	@echo "Compiled with $(CFLAGS)"

rpi: main
	@echo "Compiled with $(CFLAGS)"

$(PROG_NAME): $(OBJ)
	$(CC) -o $(PROG_NAME) $^ ${CFLAGS} $(BROTLI_LIBS_STATIC)

$(ODIR)/%.o: %.c $(DEPS)
		$(CC) -c -o $@ $< $(CFLAGS)

$(ODIR):
	mkdir $(ODIR)

#debug: ${FILES}
#	$(CC) -o $(PROG_NAME) ${FILES} ${CFLAGS} ${CFLAGS_DEBUG}

#rpi: ${FILES}
#	$(CC) ${CFLAGS_RPI} -o $(PROG_NAME) ${FILES} ${CFLAGS}


libbrotli_static: brotli_compile.sh brotli/release_$(MACH)

brotli/release_$(MACH):
	@echo -e "Clone Git repository of broli library...\nWill be installed into $(BROTLI_ROOT)"
	./brotli_compile.sh "$(MACH)" "$(CFLAGS_RPI)"

# Install and cleanup of required libs
libbrotli_dynamic: libbrotli_static
	cd brotli/release_$(MACH) && make install
	@echo "Remove brotli binary and static libs."
	@rm -f $(BROTLI_ROOT)/bin/brotli # $(BROTLI_ROOT)/lib/libbrotli*.a
	@echo "Resolve symlinks manually to halve addon size."
	@test -L "$(BROTLI_ROOT)/lib/libbrotlicommon.so" \
		&& cd "$(BROTLI_ROOT)/lib" \
		&& mv "$$(readlink "libbrotlicommon.so" )" "libbrotlicommon.so" \
		|| exit 0
	@test -L "$(BROTLI_ROOT)/lib/libbrotlidec.so" \
		&& cd "$(BROTLI_ROOT)/lib" \
		&& mv "$$(readlink "libbrotlidec.so" )" "libbrotlidec.so" \
		|| exit 0
	@test -L "$(BROTLI_ROOT)/lib/libbrotlienc.so" \
		&& cd "$(BROTLI_ROOT)/lib" \
		&& mv "$$(readlink "libbrotlienc.so" )" "libbrotlienc.so" \
		|| exit 0

#&& mv $(shell readlink $(libfile)) $(libfile)\

#mv $(BROTLI_ROOT)

# Note: depedency $(PROG_NAME) removed to avoid
# recompilation with wrong flags. (i.e. after 'make rpi' )
install: $(SCRIPT_NAME) $(COPY_TO_ARMV6L)
	mkdir -p "$(INSTALL_PREFIX)/bin"
	mkdir -p "$(INSTALL_PREFIX)/$(MACH)/bin"
	cp $(SCRIPT_NAME) "$(INSTALL_PREFIX)/bin/."
	cp $(PROG_NAME) "$(INSTALL_PREFIX)/$(MACH)/bin/."
	@echo "Program installed into $(INSTALL_PREFIX)"


# (YggdrasiI) copy build by RPi3 into directory for RPi1.
copy_to_armv6l:
	echo "Update armv6l binary, too."
	test -d ../root/armv6l || ln -s "../root/$(MACH)" "../root/armv6l"
	mkdir -p "$(INSTALL_PREFIX)/armv6l/bin"
	cp $(PROG_NAME) "$(INSTALL_PREFIX)/armv6l/bin/."

copy_lib_to_armv6l: $(BROTLI_DEP)
	@# cp -r "../root/$(MACH)/" "../root/armv6l/"
	test -d ../root/armv6l || ln -s "../root/$(MACH)" "../root/armv6l"



####################### Testing ############################

_LD_LIBRARY_PATH=$(shell echo "LD_LIBRARY_PATH=.:../root/${MACH}/lib:${LD_LIBRARY_PATH}")

TEST_ARGS=--info
ARGS ?= $(TEST_ARGS)

# Use 'ARGS='...' make run|nemiver|valgrind|leak' to propagate variables
run:
	$(_LD_LIBRARY_PATH) ./$(SCRIPT_NAME) ${ARGS}

nemiver:
	$(_LD_LIBRARY_PATH) nemiver ./$(PROG_NAME) $(ARGS)

valgrind:
	$(_LD_LIBRARY_PATH) valgrind ./$(PROG_NAME) $(ARGS)

leak:
	$(_LD_LIBRARY_PATH) valgrind  --leak-check=full ./$(PROG_NAME) $(ARGS)

memory:
	$(_LD_LIBRARY_PATH) ./val_mem.sh ./$(PROG_NAME) $(ARGS)

index: $(FILMLISTE_AKT_JSON)
	./$(SCRIPT_NAME) -a -i "$(FILMLISTE_AKT_JSON)"

search:
	./$(SCRIPT_NAME) -s --title="Livestream" -n 50 --durationMax 0 --beginMax 121m

topic:
	./$(SCRIPT_NAME) -s --topic="Tagesschau mit Gebärdensprache" -n 10 -C "ARD"

info:
	./$(SCRIPT_NAME) --info

payload:
	./$(SCRIPT_NAME) --payload 0

tests: test_indexing test1 test2 test3 test4 test5 test6 test7 test8 test9
	@echo "Tests finished. Delete $(TEST_PREFIX).test* to re-enable skipped tests."

test_indexing:
	@$(TEST_BEFORE_CMD)\
		./$(SCRIPT_NAME) update \
		$(TEST_AFTER_CMD)

test1:
	@$(TEST_BEFORE_CMD)\
		./$(SCRIPT_NAME) --search -t "Tagesschau" -n 2,0 -r --dayMax 10 -C "ARD" --durationMin "14m" \
		$(TEST_AFTER_CMD)

test2:
	@$(TEST_BEFORE_CMD)\
		./$(SCRIPT_NAME) --search -t "tagesschau" --dayMax 0 --dayMin 0 \
		$(TEST_AFTER_CMD)

test3:
	@$(TEST_BEFORE_CMD)\
		./$(SCRIPT_NAME) --search --dayMax 0 --dayMin 16 -n 1000,10000 \
		$(TEST_AFTER_CMD)

test4:
	@$(TEST_BEFORE_CMD)\
		./$(SCRIPT_NAME) --info \
		$(TEST_AFTER_CMD)

test5:
	@$(TEST_BEFORE_CMD)\
		./$(SCRIPT_NAME) --payload=0 \
		$(TEST_AFTER_CMD)

_SEARCH=--search -t "heute-show" --durationMin 30m --dayMax 21
#	@echo "Compare\n  -r $(_SEARCH) with\n     $(_SEARCH)" 
test_reversed:
	@$(TEST_BEFORE_CMD)\
		( ./$(SCRIPT_NAME) $(_SEARCH) 2>/dev/null \
		| grep '"id":' > $(TEST_PREFIX).test_reversedA ) && \
		( ./$(SCRIPT_NAME) -r $(_SEARCH) 2>/dev/null \
		| grep '"id":' | tac > $(TEST_PREFIX).test_reversedB ) && \
		diff "$(TEST_PREFIX).test_reversedA" "$(TEST_PREFIX).test_reversedB" \
		$(TEST_AFTER_CMD)

test6:
	@$(TEST_BEFORE_CMD)\
		./$(SCRIPT_NAME) -s -t "e" --durationMin 30m --sort=date -n 10 -o /dev/null \
		$(TEST_AFTER_CMD)

test7:
	@$(TEST_BEFORE_CMD)\
		./$(SCRIPT_NAME) -s -t "e" --durationMin 30m --sort=dateDesc -n 10 -o /dev/null \
		$(TEST_AFTER_CMD)

test8:
	@$(TEST_BEFORE_CMD)\
		./$(SCRIPT_NAME) -s -t "e" --durationMin 0m --sort=date -r -n 10,100000 -o /dev/null \
		$(TEST_AFTER_CMD)

test9:
	@$(TEST_BEFORE_CMD)\
		./$(SCRIPT_NAME) -s -t "e" --beginMin 20h --dayMax 10 --sort=date -n 100000 -o /dev/null \
		$(TEST_AFTER_CMD)


test_template:
	$(TEST_BEFORE_CMD) echo "Template" $(TEST_AFTER_CMD)

test_unskip_tests:
		@(ls $(TEST_PREFIX).test* 2>/dev/null && rm $(TEST_PREFIX).test* ||  exit 0)

############################################################

server: $(DATA_DIR) $(FILMLISTE_AKT_XZ) $(FILMLISTE_DIFF_XZ)
	cd ../data && python3 -m http.server

$(DATA_DIR):
	@test -d "$(DATA_DIR)" || mkdir "$(DATA_DIR)"

$(FILMLISTE_AKT_XZ):
	wget -O "$@" $(shell ./$(SCRIPT_NAME) link 2>/dev/null )

$(FILMLISTE_DIFF_XZ):
	wget -O "$@" $(shell ./$(SCRIPT_NAME) link 1 2>/dev/null )

# Download and extract the json file
download:
	./$(SCRIPT_NAME).local download 0 "$(FILMLISTE_AKT_JSON)" ||\
		./$(SCRIPT_NAME) download 0 "$(FILMLISTE_AKT_JSON)"
	@echo "\n$(FILMLISTE_AKT_JSON) saved" 

# Extract json from local copy
$(FILMLISTE_AKT_JSON): $(DATA_DIR) $(FILMLISTE_AKT_XZ)
	./$(SCRIPT_NAME).local download 0 "$@" || \
		./$(SCRIPT_NAME) download 0 "$@"

############################################################

.PHONY: clean

clean: test_unskip_tests
		rm -f $(ODIR)*/*.o *~ $(PROG_NAME)
