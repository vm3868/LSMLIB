##
## File:        Makefile.in
## Copyrights:  (c) 2005 The Trustees of Princeton University and Board of
##                  Regents of the University of Texas.  All rights reserved.
##              (c) 2009 Kevin T. Chu.  All rights reserved.
## Revision:    $Revision$
## Modified:    $Date$
## Description: makefile for fast marching method functions
##

SRC_DIR = .

BUILD_DIR = ../../..
include $(BUILD_DIR)/config/Makefile.config

all:  includes library

includes:  
	cp -f $(SRC_DIR)/FMM_Core.h $(BUILD_DIR)/include/
	cp -f $(SRC_DIR)/FMM_Heap.h $(BUILD_DIR)/include/
	cp -f $(SRC_DIR)/FMM_Callback_API.h $(BUILD_DIR)/include/
	cp -f $(SRC_DIR)/FMM_Macros.h $(BUILD_DIR)/include/

library:  FMM_Heap.o       \
          FMM_Core.o

clean:
	rm -f *.o 

include Makefile.depend

