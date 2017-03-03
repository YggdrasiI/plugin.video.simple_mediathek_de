############################################################

SCRIPT_NAME=simple_mediathek
PROG_NAME=$(SCRIPT_NAME).bin
BROTLI=1
TEST_ARGS=-s --dayMin 0 --dayMax 7 --title="\"extra 3\""

# Scripts will installed in $INSTALL_PREFIX/bin
# Binaries will insatlled in $INSTALL_PREFIX/$MACH
INSTALL_PREFIX=../root
MACH=$(shell uname -m)

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

############################################################

FILES=main.c helper.c hashmap.c utf8.c \
			parser.c filmliste.c channels.c linked_list.c \
			params.c search.c payload.c info.c \
			search_output.c

DEPS=settings.h
#DEPS=$$(ls *.h)

ifneq ($(BROTLI),0)
BROTLI_ROOT=${INSTALL_PREFIX}/${MACH}
BROTLI_DEP=$(BROTLI_ROOT)/lib/libbrotlidec.so
# With Brotli
CFLAGS+=-DCOMPRESS_BROTLI -I$(BROTLI_ROOT)/include  -L$(BROTLI_ROOT)/lib -lbrotlidec -lbrotlienc -lbrotlicommon
#CFLAGS+=-DCOMPRESS_BROTLI $$(PKG_CONFIG_PATH=$(BROTLI_ROOT)/lib/pkgconfig/ pkg-config --libs --cflags brotli)
FILES+=brotli.c
else
BROTLI_DEP=Makefile
endif

#_OBJ = $(patsubst %.c, %.o, $(FILES))
OBJ = $(FILES:%.c=$(ODIR)/%.o)

############################################################

main: $(BROTLI_DEP) $(ODIR) $(PROG_NAME)
	@echo "Done"

debug: main
	@echo "Compiled with $(CFLAGS)"

rpi: main
	@echo "Compiled with $(CFLAGS)"

$(PROG_NAME): $(OBJ)
	$(CC) -o $(PROG_NAME) $^ ${CFLAGS}

$(ODIR)/%.o: %.c $(DEPS)
		$(CC) -c -o $@ $< $(CFLAGS)

$(ODIR):
	mkdir $(ODIR)

#debug: ${FILES}
#	$(CC) -o $(PROG_NAME) ${FILES} ${CFLAGS} ${CFLAGS_DEBUG}

#rpi: ${FILES}
#	$(CC) ${CFLAGS_RPI} -o $(PROG_NAME) ${FILES} ${CFLAGS}


$(BROTLI_DEP): brotli_install.sh
	@echo -e "Clone Git repository of broli library...\nWill be installed into $(BROTLI_ROOT)"
	./brotli_install.sh "$(MACH)" "$(CFLAGS_RPI)"
		
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

nemiver:
	nemiver ./$(SCRIPT_NAME) $(TEST_ARGS)

valgrind:
	valgrind ./$(SCRIPT_NAME) $(TEST_ARGS)

leak:
	valgrind  --leak-check=full ./$(SCRIPT_NAME) $(TEST_ARGS)

test:
	./$(SCRIPT_NAME) $(TEST_ARGS)

index:
	./$(SCRIPT_NAME) -a -i /dev/shm/filme.json

info:
	./$(SCRIPT_NAME) --info

payload:
	./$(SCRIPT_NAME) --payload 0

.PHONY: clean

clean:
		rm -f $(ODIR)*/*.o *~ $(PROG_NAME)
