tensorMgr.o: tensorMgr.cpp tensorMgr.h qtensor.h ../../include/phase.h \
 ../../include/myConcepts.h ../../include/rationalNumber.h \
 ../../include/util.h ../../include/myUsage.h ../../include/rnGen.h \
 tensor.h tensorDef.h tensorUtil.h ../../include/util.h
tensorUtil.o: tensorUtil.cpp tensorUtil.h
tensorCmd.o: tensorCmd.cpp tensorCmd.h ../../include/cmdParser.h \
 ../../include/cmdCharDef.h ../../include/cmdMacros.h qtensor.h \
 ../../include/phase.h ../../include/myConcepts.h \
 ../../include/rationalNumber.h ../../include/util.h \
 ../../include/myUsage.h ../../include/rnGen.h tensor.h tensorDef.h \
 tensorUtil.h ../../include/util.h tensorMgr.h ../../include/textFormat.h
