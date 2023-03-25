EXTINCDIR 		:= include
DEPENDDIR 		:= -I. -I$(EXTINCDIR)
MK_INCLUDE_DIR	:= include/mk

CXX       		:= g++
CCC       		:= gcc
AR        		:= ar cr
ECHO      		:= /bin/echo

OPTIMIZE_LEVEL 	:= -g
DEBUG_FLAG 		:= -DDEBUG
DEP_FLAG		:= -MMD

# debug: OPTIMIZE_LEVEL := -g
# release: DEBUG_FLAG := -DNDEBUG

LIBPKGS   = $(REFPKGS) $(SRCPKGS)

# OS detection
# https://stackoverflow.com/questions/714100/os-detecting-makefile
ifeq ($(OS),Windows_NT)
    OSFLAG += -DWIN32
    ifeq ($(PROCESSOR_ARCHITEW6432),AMD64)
        OSFLAG += -DAMD64
    else
        ifeq ($(PROCESSOR_ARCHITECTURE),AMD64)
            OSFLAG += -DAMD64
        endif
        ifeq ($(PROCESSOR_ARCHITECTURE),x86)
            OSFLAG += -DIA32
        endif
    endif
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        OSFLAG += -DLINUX
    endif
    ifeq ($(UNAME_S),Darwin)
        OSFLAG += -DOSX
    endif
    UNAME_P := $(shell uname -p)
    ifeq ($(UNAME_P),x86_64)
        OSFLAG += -DAMD64
    endif
    ifneq ($(filter %86,$(UNAME_P)),)
        OSFLAG += -DIA32
    endif
    ifneq ($(filter arm%,$(UNAME_P)),)
        OSFLAG += -DARM
    endif
endif

ifneq (,$(findstring -DOSX, $(OSFLAG)))
    LIBPKGS += lapack cblas blas
else
    LIBPKGS += lapack cblas blas gfortran
endif

## -------------------------------
##      include local rules
## -------------------------------

define INCLUDE_DIR
DIR := $(MODULE)
include $(SRC_DIR)/$(MODULE)/rules.mk

endef

$(foreach MODULE, $(SRCPKGS),$(eval $(INCLUDE_DIR)))

DIR := main
TARGET := $(BUILD_DIR)/$(EXEC)
include $(SRC_DIR)/$(DIR)/rules.mk

DIR := test
TARGET := $(BUILD_DIR)/$(TESTEXEC)
include $(SRC_DIR)/$(DIR)/rules.mk

CFLAGS 			:= $(OPTIMIZE_LEVEL) $(DEBUG_FLAG) $(DEP_FLAG) -Wall -std=c++20 -fopenmp $(PKGFLAG)

## -------------------------------
##      Generic build policy
## -------------------------------

$(BUILD_SRC_DIR)/%.o : $(SRC_DIR)/%.cpp $(EXTHEADER_MKS)
	@-mkdir -p $(@D)
	@$(ECHO) "> compiling: $(notdir $<)"
	@$(CXX) $(CFLAGS) -I$(EXTINCDIR) -c -o $@ $<

$(BUILD_SRC_DIR)/%.o : $(SRC_DIR)/%.c $(EXTHEADER_MKS)
	@-mkdir -p $(@D)
	@$(ECHO) "> compiling: $(notdir $<)"
	@$(CCC) $(CFLAGS) -I$(EXTINCDIR) -c -o $@ $<
