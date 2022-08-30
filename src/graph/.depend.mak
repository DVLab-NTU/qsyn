zxCmd.o: zxCmd.cpp zxCmd.h ../../include/cmdParser.h \
 ../../include/cmdCharDef.h zxGraph.h zxGraphMgr.h zxDef.h \
 ../../include/myHashMap.h ../../include/util.h ../../include/rnGen.h \
 ../../include/myUsage.h
zxGraphMgr.o: zxGraphMgr.cpp zxGraphMgr.h zxGraph.h zxDef.h \
 ../../include/myHashMap.h
zxGraph.o: zxGraph.cpp zxGraph.h ../../include/util.h \
 ../../include/rnGen.h ../../include/myUsage.h
