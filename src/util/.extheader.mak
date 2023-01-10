util.d: ../../include/util.h ../../include/rnGen.h ../../include/myUsage.h ../../include/rationalNumber.h ../../include/phase.h ../../include/myConcepts.h ../../include/textFormat.h ../../include/ordered_hashmap.h ../../include/ordered_hashset.h ../../include/ordered_hashtable.h 
../../include/util.h: util.h
	@rm -f ../../include/util.h
	@ln -fs ../src/util/util.h ../../include/util.h
../../include/rnGen.h: rnGen.h
	@rm -f ../../include/rnGen.h
	@ln -fs ../src/util/rnGen.h ../../include/rnGen.h
../../include/myUsage.h: myUsage.h
	@rm -f ../../include/myUsage.h
	@ln -fs ../src/util/myUsage.h ../../include/myUsage.h
../../include/rationalNumber.h: rationalNumber.h
	@rm -f ../../include/rationalNumber.h
	@ln -fs ../src/util/rationalNumber.h ../../include/rationalNumber.h
../../include/phase.h: phase.h
	@rm -f ../../include/phase.h
	@ln -fs ../src/util/phase.h ../../include/phase.h
../../include/myConcepts.h: myConcepts.h
	@rm -f ../../include/myConcepts.h
	@ln -fs ../src/util/myConcepts.h ../../include/myConcepts.h
../../include/textFormat.h: textFormat.h
	@rm -f ../../include/textFormat.h
	@ln -fs ../src/util/textFormat.h ../../include/textFormat.h
../../include/ordered_hashmap.h: ordered_hashmap.h
	@rm -f ../../include/ordered_hashmap.h
	@ln -fs ../src/util/ordered_hashmap.h ../../include/ordered_hashmap.h
../../include/ordered_hashset.h: ordered_hashset.h
	@rm -f ../../include/ordered_hashset.h
	@ln -fs ../src/util/ordered_hashset.h ../../include/ordered_hashset.h
../../include/ordered_hashtable.h: ordered_hashtable.h
	@rm -f ../../include/ordered_hashtable.h
	@ln -fs ../src/util/ordered_hashtable.h ../../include/ordered_hashtable.h
