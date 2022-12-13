extractor.d: ../../include/extract.h 
../../include/extract.h: extract.h
	@rm -f ../../include/extract.h
	@ln -fs ../src/extractor/extract.h ../../include/extract.h
