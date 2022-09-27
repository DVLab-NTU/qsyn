testTensor.o: testTensor.cpp ../../include/qtensor.h \
 ../../include/tensor.h ../../include/tensorDef.h ../../include/util.h \
 ../../include/rnGen.h ../../include/myUsage.h ../../include/tensorUtil.h \
 ../../include/phase.h ../../include/rationalNumber.h \
 ../../include/myConcepts.h ../../include/util.h \
 ../../include/catch2/catch.hpp
testRational.o: testRational.cpp ../../include/rationalNumber.h \
 ../../include/catch2/catch.hpp
testPhase.o: testPhase.cpp ../../include/phase.h \
 ../../include/rationalNumber.h ../../include/myConcepts.h \
 ../../include/util.h ../../include/rnGen.h ../../include/myUsage.h \
 ../../include/catch2/catch.hpp
tests.o: tests.cpp ../../include/catch2/catch.hpp
