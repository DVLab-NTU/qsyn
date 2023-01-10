tensorMgr.o: tensorMgr.cpp tensorMgr.h ../../include/phase.h \
 ../../include/myConcepts.h ../../include/rationalNumber.h \
 ../../include/util.h ../../include/myUsage.h ../../include/rnGen.h \
 qtensor.h tensor.h tensorDef.h tensorUtil.h ../../include/util.h
tensorUtil.o: tensorUtil.cpp tensorUtil.h
tensorCmd.o: tensorCmd.cpp tensorCmd.h ../../include/cmdParser.h \
 ../../include/cmdCharDef.h ../../include/phase.h \
 ../../include/myConcepts.h ../../include/rationalNumber.h \
 ../../include/util.h ../../include/myUsage.h ../../include/rnGen.h \
 tensorMgr.h ../../include/textFormat.h
