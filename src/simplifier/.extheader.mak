simplifier.d: ../../include/simplify.h ../../include/zxRules.h 
../../include/simplify.h: simplify.h
	@rm -f ../../include/simplify.h
	@ln -fs ../src/simplifier/simplify.h ../../include/simplify.h
../../include/zxRules.h: zxRules.h
	@rm -f ../../include/zxRules.h
	@ln -fs ../src/simplifier/zxRules.h ../../include/zxRules.h
