qcir.d: ../../include/qcirMgr.h ../../include/qcir.h ../../include/qcirGate.h ../../include/qcirQubit.h ../../include/qcirDef.h ../../include/qcirCmd.h 
../../include/qcirMgr.h: qcirMgr.h
	@rm -f ../../include/qcirMgr.h
	@ln -fs ../src/qcir/qcirMgr.h ../../include/qcirMgr.h
../../include/qcir.h: qcir.h
	@rm -f ../../include/qcir.h
	@ln -fs ../src/qcir/qcir.h ../../include/qcir.h
../../include/qcirGate.h: qcirGate.h
	@rm -f ../../include/qcirGate.h
	@ln -fs ../src/qcir/qcirGate.h ../../include/qcirGate.h
../../include/qcirQubit.h: qcirQubit.h
	@rm -f ../../include/qcirQubit.h
	@ln -fs ../src/qcir/qcirQubit.h ../../include/qcirQubit.h
../../include/qcirDef.h: qcirDef.h
	@rm -f ../../include/qcirDef.h
	@ln -fs ../src/qcir/qcirDef.h ../../include/qcirDef.h
../../include/qcirCmd.h: qcirCmd.h
	@rm -f ../../include/qcirCmd.h
	@ln -fs ../src/qcir/qcirCmd.h ../../include/qcirCmd.h
