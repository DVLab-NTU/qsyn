cmd.d: ../../include/cmdParser.h ../../include/cmdCharDef.h 
../../include/cmdParser.h: cmdParser.h
	@rm -f ../../include/cmdParser.h
	@ln -fs ../src/cmd/cmdParser.h ../../include/cmdParser.h
../../include/cmdCharDef.h: cmdCharDef.h
	@rm -f ../../include/cmdCharDef.h
	@ln -fs ../src/cmd/cmdCharDef.h ../../include/cmdCharDef.h
