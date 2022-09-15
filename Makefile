REFPKGS   = 
SRCPKGS   = cmd qcir graph tensor util
LIBPKGS   = $(REFPKGS) $(SRCPKGS)
MAIN      = main
TESTMAIN  = test

LIBS      = $(addprefix -l, $(LIBPKGS))
SRCLIBS   = $(addsuffix .a, $(addprefix lib, $(SRCPKGS)))

EXEC      = qsyn
TESTEXEC  = tests

all:  main
test: testmain

libs:
	@for pkg in $(SRCPKGS); \
	do \
		echo "Checking $$pkg..."; \
		cd src/$$pkg; $(MAKE) -f make.$$pkg --no-print-directory PKGNAME=$$pkg; \
		cd ../..; \
	done

main: libs
	@echo "Checking $(MAIN)..."
	@cd src/$(MAIN); \
		$(MAKE) -f make.$(MAIN) --no-print-directory INCLIB="$(LIBS)" EXEC=$(EXEC);
	@ln -fs bin/$(EXEC) .
#	@strip bin/$(EXEC)

testmain: libs
	@export LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:/usr/local/lib
	@echo "Checking $(TESTMAIN)..."
	@cd src/$(TESTMAIN); \
		$(MAKE) -f make.$(TESTMAIN) --no-print-directory INCLIB="$(LIBS)" EXEC=$(TESTEXEC);


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

linux18 mac:
	@for pkg in $(REFPKGS); \
	do \
	        cd lib; ln -sf lib$$pkg-$@.a lib$$pkg.a; cd ../..; \
	done
