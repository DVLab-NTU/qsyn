REFPKGS   = 
SRCPKGS   = extractor qcir simplifier gflow m2 graph tensor util cmd
LIBPKGS   = $(REFPKGS) $(SRCPKGS)
MAIN      = main
TESTMAIN  = test

LIBS      = $(addprefix -l, $(LIBPKGS))
SRCLIBS   = $(addsuffix .a, $(addprefix lib, $(SRCPKGS)))

EXEC      = qsyn
TESTEXEC  = tests

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

all:  main
test: testmain

libs:
	@for pkg in $(SRCPKGS); \
	do \
		echo "Checking $$pkg..."; \
		cd src/$$pkg; $(MAKE) -f make.$$pkg --no-print-directory PKGNAME=$$pkg OSFLAG="$(OSFLAG)"; \
		cd ../..; \
	done

main: libs
	@echo "Checking $(MAIN)..."
	@cd src/$(MAIN); \
		$(MAKE) -f make.$(MAIN) --no-print-directory INCLIB="$(LIBS)" EXEC=$(EXEC) OSFLAG="$(OSFLAG)";
	@ln -fs bin/$(EXEC) .
#	@strip bin/$(EXEC)

testmain: libs
	@export LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:/usr/local/lib
	@echo "Checking $(TESTMAIN)..."
	@cd src/$(TESTMAIN); \
		$(MAKE) -f make.$(TESTMAIN) --no-print-directory INCLIB="$(LIBS)" EXEC=$(TESTEXEC) OSFLAG="$(OSFLAG)";


clean:
	@for pkg in $(SRCPKGS); \
	do \
		echo "Cleaning $$pkg..."; \
		cd src/$$pkg; $(MAKE) -f make.$$pkg --no-print-directory PKGNAME=$$pkg clean; \
                cd ../..; \
	done
	@echo "Cleaning $(MAIN)..."
	@cd src/$(MAIN); $(MAKE) -f make.$(MAIN) --no-print-directory clean
	@echo "Cleaning $(TESTMAIN)..."
	@cd src/$(TESTMAIN); $(MAKE) -f make.$(TESTMAIN) --no-print-directory clean
	@echo "Removing $(SRCLIBS)..."
	@cd lib; rm -f $(SRCLIBS)
	@echo "Removing $(EXEC)..."
	@rm -f bin/$(EXEC)

cleanall: clean
	@echo "Removing bin/*..."
	@rm -rf bin/*

ctags:	  
	@rm -f src/tags
	@for pkg in $(SRCPKGS); \
	do \
		echo "Tagging $$pkg..."; \
		cd src; ctags -a $$pkg/*.cpp $$pkg/*.h; cd ..; \
	done
	@echo "Tagging $(MAIN)..."
	@cd src; ctags -a $(MAIN)/*.cpp $(MAIN)/*.h
