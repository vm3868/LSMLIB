##
## File:        Makefile.in
## Copyrights:  (c) 2005 The Trustees of Princeton University and Board of
##                  Regents of the University of Texas.  All rights reserved.
##              (c) 2009 Kevin T. Chu.  All rights reserved.
## Revision:    $Revision$
## Modified:    $Date$
## Description: makefile for reinitialization example
##

BUILD_DIR = ../../..
include $(BUILD_DIR)/config/Makefile.config

INCLUDE      = -I$(LSMLIB_INCLUDE) -I.
LIB_DIRS     = -L$(LSMLIB_LIB_DIR) -L.

# uncomment if gdb debugging flags are desired
#CFLAGS_EXTRA = -g
#FFLAGS_EXTRA = -g

PROGS = lsm_reinitialize
OBJLIST = lsm_options.o lsm_reinitialization_medium2d.o lsm_reinitialize.o

all:  $(PROGS)

lsm_reinitialize: $(OBJLIST)
	gcc -O3 -funroll-loops -fPIC -g -O2 $(CFLAGS_EXTRA) $(LIB_DIRS) $(INCLUDE) $(OBJLIST) \
	$(LSMLIB_LIBS) -o $@

clean:
	rm -f *.o
	rm -f $(PROGS)
	
spotless:  clean
	rm -f data_final*
	rm -f data_init*
	rm -f grid*
	rm -f mask*
	rm -f out_file*
