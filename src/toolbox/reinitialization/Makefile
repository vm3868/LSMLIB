##
## File:        Makefile.in
## Copyrights:  (c) 2005 The Trustees of Princeton University and Board of
##                  Regents of the University of Texas.  All rights reserved.
##              (c) 2009 Kevin T. Chu.  All rights reserved.
## Revision:    $Revision$
## Modified:    $Date$
## Description: makefile for reinitialization subroutines
##

SRC_DIR = .

BUILD_DIR = ../../..
include $(BUILD_DIR)/config/Makefile.config

all:  includes library

includes:  
	cp -f $(SRC_DIR)/lsm_reinitialization1d.h $(BUILD_DIR)/include/
	cp -f $(SRC_DIR)/lsm_reinitialization2d.h $(BUILD_DIR)/include/
	cp -f $(SRC_DIR)/lsm_reinitialization2d_local.h $(BUILD_DIR)/include/
	cp -f $(SRC_DIR)/lsm_reinitialization3d.h $(BUILD_DIR)/include/
	cp -f $(SRC_DIR)/lsm_reinitialization3d_local.h $(BUILD_DIR)/include/

library:  lsm_reinitialization1d.o         \
          lsm_reinitialization2d.o         \
          lsm_reinitialization2d_local.o   \
          lsm_reinitialization3d.o         \
          lsm_reinitialization3d_local.o

clean:
	rm -f *.o 

