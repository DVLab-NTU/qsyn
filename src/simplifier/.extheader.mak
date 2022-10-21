simplifier.d: ../../include/simplify.h ../../include/zxRules.h ../../include/simpCmd.h 
../../include/simplify.h: simplify.h
	@rm -f ../../include/simplify.h
	@ln -fs ../src/simplifier/simplify.h ../../include/simplify.h
../../include/zxRules.h: zxRules.h
	@rm -f ../../include/zxRules.h
	@ln -fs ../src/simplifier/zxRules.h ../../include/zxRules.h
../../include/simpCmd.h: simpCmd.h
	@rm -f ../../include/simpCmd.h
	@ln -fs ../src/simplifier/simpCmd.h ../../include/simpCmd.h
