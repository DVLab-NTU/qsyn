util.d: ../../include/util.h ../../include/rnGen.h ../../include/myUsage.h ../../include/myHashMap.h ../../include/rationalNumber.h ../../include/phase.h ../../include/myConcepts.h ../../include/nestedInitializerList.h 
../../include/util.h: util.h
	@rm -f ../../include/util.h
	@ln -fs ../src/util/util.h ../../include/util.h
../../include/rnGen.h: rnGen.h
	@rm -f ../../include/rnGen.h
	@ln -fs ../src/util/rnGen.h ../../include/rnGen.h
../../include/myUsage.h: myUsage.h
	@rm -f ../../include/myUsage.h
	@ln -fs ../src/util/myUsage.h ../../include/myUsage.h
../../include/myHashMap.h: myHashMap.h
	@rm -f ../../include/myHashMap.h
	@ln -fs ../src/util/myHashMap.h ../../include/myHashMap.h
../../include/rationalNumber.h: rationalNumber.h
	@rm -f ../../include/rationalNumber.h
	@ln -fs ../src/util/rationalNumber.h ../../include/rationalNumber.h
../../include/phase.h: phase.h
	@rm -f ../../include/phase.h
	@ln -fs ../src/util/phase.h ../../include/phase.h
../../include/myConcepts.h: myConcepts.h
	@rm -f ../../include/myConcepts.h
	@ln -fs ../src/util/myConcepts.h ../../include/myConcepts.h
../../include/nestedInitializerList.h: nestedInitializerList.h
	@rm -f ../../include/nestedInitializerList.h
	@ln -fs ../src/util/nestedInitializerList.h ../../include/nestedInitializerList.h
