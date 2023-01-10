cmdCharDef.o: cmdCharDef.cpp cmdCharDef.h cmdParser.h
cmdReader.o: cmdReader.cpp cmdCharDef.h cmdParser.h
cmdCommon.o: cmdCommon.cpp cmdCommon.h cmdParser.h cmdCharDef.h \
 ../../include/myUsage.h ../../include/util.h ../../include/myUsage.h \
 ../../include/rnGen.h
cmdParser.o: cmdParser.cpp cmdParser.h cmdCharDef.h ../../include/util.h \
 ../../include/myUsage.h ../../include/rnGen.h
