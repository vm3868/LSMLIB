##
## File:        Makefile.in
## Copyrights:  (c) 2005 The Trustees of Princeton University and Board of
##                  Regents of the University of Texas.  All rights reserved.
##              (c) 2009 Kevin T. Chu.  All rights reserved.
## Revision:    $Revision$
## Modified:    $Date$
## Description: makefile for SAMRAI Fortran 77 utility subroutines
##

SRC_DIR = .

BUILD_DIR = ../../..
include $(BUILD_DIR)/config/Makefile.config

# Set MAKE


all:  includes library

includes:  
	cp -f $(SRC_DIR)/lsm_samrai_f77_utilities.h $(BUILD_DIR)/include/

library:  lsm_samrai_f77_utilities.o

clean:
		rm -f *.o 

