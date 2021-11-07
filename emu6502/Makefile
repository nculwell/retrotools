
ARCH = -m32
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Werror $(ARCH)
LDFLAGS = $(ARCH)
HEADERS = em.h emtrace.h
#DEBUG_OPT = -Og
DEBUG_OPT = -O0
MAX_OPT = -Os
#MAX_OPT = -O3
EXECUTABLES = c64emulator forth_decompiler

debug : CFLAGS += -g $(DEBUG_OPT)
debug : #LDFLAGS += -lefence
debug : all

opt : CFLAGS += $(MAX_OPT) -DTRACE_OFF
opt : all

all : $(EXECUTABLES)

c64emulator : c64emulator.o \
  emmain.o emdisk.o instruct.o trackinfo.o file.o ecaloader.o emromc64.o

forth_decompiler: forth_decompiler.o

c64emulator.o : c64emulator.c $(HEADERS)
emromc64.o : emromc64.c $(HEADERS)
emmain.o : emmain.c $(HEADERS)
emdisk.o : emdisk.c $(HEADERS)
instruct.o : instruct.c instrdef.inc $(HEADERS)
trackinfo.o : trackinfo.c $(HEADERS)
file.o : file.c $(HEADERS)
ecaloader.o : ecaloader.c ecalabels.c $(HEADERS)
instrdef.inc : codegen instset.tbl
	./codegen instruction_set instset.tbl instrdef.inc
codegen : codegen.o

FORTH_DICT_INC_FILES = forth_words_addrs.inc  forth_words_defs.inc  forth_words_names.inc
forth_decompiler.o: forth_decompiler.c $(FORTH_DICT_INC_FILES)
$(FORTH_DICT_INC_FILES): forth_words.txt gen_forth_dict.py
	./gen_forth_dict.py

clean:
	$(RM) $(EXECUTABLES)
	$(RM) *.o
	$(RM) *.inc

