cmdCharDef.o: cmdCharDef.cpp cmdParser.h cmdCharDef.h cmdMacros.h
cmdReader.o: cmdReader.cpp cmdParser.h cmdCharDef.h cmdMacros.h \
 ../../include/util.h ../../include/myUsage.h ../../include/rnGen.h
cmdCommon.o: cmdCommon.cpp cmdCommon.h cmdParser.h cmdCharDef.h \
 cmdMacros.h ../../include/util.h ../../include/myUsage.h \
 ../../include/rnGen.h
cmdParser.o: cmdParser.cpp cmdParser.h cmdCharDef.h cmdMacros.h \
 ../../include/util.h ../../include/myUsage.h ../../include/rnGen.h
