cmdCharDef.o: cmdCharDef.cpp cmdParser.h cmdCharDef.h
cmdReader.o: cmdReader.cpp ../../include/util.h ../../include/rnGen.h \
 ../../include/myUsage.h cmdParser.h cmdCharDef.h
cmdCommon.o: cmdCommon.cpp ../../include/util.h ../../include/rnGen.h \
 ../../include/myUsage.h cmdCommon.h cmdParser.h cmdCharDef.h
cmdParser.o: cmdParser.cpp ../../include/util.h ../../include/rnGen.h \
 ../../include/myUsage.h cmdParser.h cmdCharDef.h
