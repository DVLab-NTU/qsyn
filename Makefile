REFPKGS   		:= 
SRCPKGS   		:= extractor optimizer duostra device qcir simplifier lattice gflow m2 graph tensor cmd argparse util
SRCLIBS   		:= $(addsuffix .a, $(addprefix lib, $(SRCPKGS)))

EXTINCDIR 		:= include

VENDOR_DIR      := vendor
VENDOR_HDRS		:= tqdm

SRC_DIR	  		:= src
BUILD_DIR 		:= bin/qsyn-dev
BUILD_SRC_DIR 	:= $(BUILD_DIR)/$(SRC_DIR)
LIB_DIR    		:= $(BUILD_DIR)/lib

EXEC     		:= qsyn

OPTIMIZE_LEVEL 	:= -O3
DEBUG_FLAG 		:= -DDEBUG

.PHONY: all

all:  main

## Clean all objects files
.PHONY: clean

clean: $(addprefix clean_, $(SRCPKGS) main)

## Clean all objects files, .depend.mk, extheader.mk, and include/*
.PHONY: cleanall

cleanall: clean
	@echo "Removing bin/*..."
	@rm -rf bin/*
	@echo "Removing include/*.h..."
	@rm -f include/*.h
	@echo "Removing vendor header links..."
	@rm -f $(VENDOR_LINKS)

## lint the source files in-place
.PHONY: lint

lint:
	@find ./src -regex ".*\.\(h\|cpp\)" -type f | xargs clang-format -i

.PHONY: libs

libs: $(addprefix $(BUILD_SRC_DIR), $(SRCLIBS))

## Linking external headers
.PHONY: extheader
EXTHEADER_MKS := $(addsuffix /.extheader.mk, $(addprefix $(BUILD_SRC_DIR)/, $(SRCPKGS) main))

extheader: $(EXTHEADER_MKS)

.PHONY: vendor-headers

VENDOR_LINKS := $(addprefix $(EXTINCDIR)/, $(VENDOR_HDRS))
vendor-headers: $(VENDOR_LINKS)


$(VENDOR_LINKS):
	@$(ECHO) "Linking vendor header $(@F)..."
	@rm -f $@
	@ln -s ../$(VENDOR_DIR)/$@ $@

include $(SRC_DIR)/rules.mk
