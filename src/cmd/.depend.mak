cmdCharDef.o: cmdCharDef.cpp cmdParser.h cmdCharDef.h cmdMacros.h
cmdReader.o: cmdReader.cpp ../../include/util.h ../../include/rnGen.h \
 ../../include/myUsage.h cmdParser.h cmdCharDef.h cmdMacros.h
cmdCommon.o: cmdCommon.cpp ../../include/util.h ../../include/rnGen.h \
 ../../include/myUsage.h cmdCommon.h cmdParser.h cmdCharDef.h cmdMacros.h
cmdParser.o: cmdParser.cpp cmdParser.h cmdCharDef.h cmdMacros.h \
 ../../include/util.h ../../include/rnGen.h ../../include/myUsage.h
