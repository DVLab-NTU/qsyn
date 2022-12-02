m2.d: ../../include/m2.h ../../include/m2Def.h 
../../include/m2.h: m2.h
	@rm -f ../../include/m2.h
	@ln -fs ../src/m2/m2.h ../../include/m2.h
../../include/m2Def.h: m2Def.h
	@rm -f ../../include/m2Def.h
	@ln -fs ../src/m2/m2Def.h ../../include/m2Def.h
