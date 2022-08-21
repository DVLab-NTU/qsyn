qcirGate.o: qcirGate.cpp qcirMgr.h qcirGate.h qcirDef.h \
 ../../include/myHashMap.h
qcirMgr.o: qcirMgr.cpp qcirMgr.h qcirGate.h qcirDef.h \
 ../../include/myHashMap.h
qcirCmd.o: qcirCmd.cpp qcirMgr.h qcirGate.h qcirDef.h \
 ../../include/myHashMap.h qcirCmd.h ../../include/cmdParser.h \
 ../../include/cmdCharDef.h ../../include/util.h ../../include/rnGen.h \
 ../../include/myUsage.h
