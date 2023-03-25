REFPKGS   		:= 
SRCPKGS   		:= argparse extractor topology qcir simplifier lattice gflow m2 graph tensor cmd util
SRCLIBS   		:= $(addsuffix .a, $(addprefix lib, $(SRCPKGS)))

SRC_DIR	  		:= src
BUILD_DIR 		:= bin/qsyn-dev
BUILD_SRC_DIR 	:= $(BUILD_DIR)/$(SRC_DIR)
LIBDIR    		:= $(BUILD_DIR)/lib

EXEC     		:= qsyn
TESTEXEC  		:= qsyn-test

.PHONY: all

all:  main

## Clean all objects files
.PHONY: clean

clean: $(addprefix clean_, $(SRCPKGS) main test)

## Clean all objects files, .depend.mk, extheader.mk, and include/*
.PHONY: cleanall

cleanall: clean
	@echo "Removing bin/*..."
	@rm -rf bin/*
	@echo "Removing include/*.h..."
	@rm -f include/*.h

lint:
	@find ./src -regex ".*\.\(h\|cpp\)" -type f | xargs clang-format -i

.PHONY: libs

libs: $(addprefix $(BUILD_SRC_DIR), $(SRCLIBS))

## Linking external headers
.PHONY: extheader
EXTHEADER_MKS := $(addsuffix /.extheader.mk, $(addprefix $(BUILD_SRC_DIR)/, $(SRCPKGS) main test))

# extheader: $(addsuffix /.extheader.mk, $(addprefix $(BUILD_SRC_DIR)/, $(SRCPKGS) main test))
extheader: $(EXTHEADER_MKS)
include $(SRC_DIR)/rules.mk
