##
## File:        Makefile.in
## Copyrights:  (c) 2005 The Trustees of Princeton University and Board of
##                  Regents of the University of Texas.  All rights reserved.
##              (c) 2009 Kevin T. Chu.  All rights reserved.
## Revision:    $Revision$
## Modified:    $Date$
## Description: makefile for level set method example code
##

BUILD_DIR = ..
include $(BUILD_DIR)/config/Makefile.config

# Set MAKE


SUBDIRS = serial       \
          parallel     \
          toolbox

all:  

library: 

clean:
	for DIR in $(SUBDIRS); do (cd $$DIR; make $@) || exit 1; done

spotless: clean
	for DIR in $(SUBDIRS); do (cd $$DIR; make $@) || exit 1; done

