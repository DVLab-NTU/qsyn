qcir.d: ../../include/qcirDef.h 
../../include/qcirDef.h: qcirDef.h
	@rm -f ../../include/qcirDef.h
	@ln -fs ../src/qcir/qcirDef.h ../../include/qcirDef.h
