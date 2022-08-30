REFPKGS   = cmd
SRCPKGS   = qcir util graph
LIBPKGS   = $(REFPKGS) $(SRCPKGS)
MAIN      = main
TESTMAIN  = test

LIBS      = $(addprefix -l, $(LIBPKGS))
SRCLIBS   = $(addsuffix .a, $(addprefix lib, $(SRCPKGS)))

EXEC      = qsyn
TESTEXECS = testRational test2

all:  libs main
test: libs testmain

libs:
	@for pkg in $(SRCPKGS); \
	do \
		echo "Checking $$pkg..."; \
		cd src/$$pkg; make -f make.$$pkg --no-print-directory PKGNAME=$$pkg; \
		cd ../..; \
	done

main:
	@echo "Checking $(MAIN)..."
	@cd src/$(MAIN); \
		make -f make.$(MAIN) --no-print-directory INCLIB="$(LIBS)" EXEC=$(EXEC);
	@ln -fs bin/$(EXEC) .
#	@strip bin/$(EXEC)

testmain:
	@echo "Checking $(TESTMAIN)..."
	@for testexec in $(TESTEXECS); \
	do \
		cd src/$(TESTMAIN); \
		make -f make.$(TESTMAIN) --no-print-directory INCLIB="$(LIBS)" EXEC=$$testexec; \
		cd ../../tests; \
		ln -fs ../bin/$$testexec .; \
		cd ..; \
	done


clean:
	@for pkg in $(SRCPKGS); \
	do \
		echo "Cleaning $$pkg..."; \
		cd src/$$pkg; make -f make.$$pkg --no-print-directory PKGNAME=$$pkg clean; \
                cd ../..; \
	done
	@echo "Cleaning $(MAIN)..."
	@cd src/$(MAIN); make -f make.$(MAIN) --no-print-directory clean
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

linux18 linux16 mac:
	@for pkg in $(REFPKGS); \
	do \
	        cd lib; ln -sf lib$$pkg-$@.a lib$$pkg.a; cd ../..; \
	done
