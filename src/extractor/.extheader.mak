extractor.d: ../../include/extract.h ../../include/extractorCmd.h 
../../include/extract.h: extract.h
	@rm -f ../../include/extract.h
	@ln -fs ../src/extractor/extract.h ../../include/extract.h
../../include/extractorCmd.h: extractorCmd.h
	@rm -f ../../include/extractorCmd.h
	@ln -fs ../src/extractor/extractorCmd.h ../../include/extractorCmd.h
