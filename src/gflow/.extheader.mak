gflow.d: ../../include/gFlow.h 
../../include/gFlow.h: gFlow.h
	@rm -f ../../include/gFlow.h
	@ln -fs ../src/gflow/gFlow.h ../../include/gFlow.h
