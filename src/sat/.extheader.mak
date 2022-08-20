sat.d: ../../include/sat.h ../../include/Solver.h ../../include/SolverTypes.h ../../include/VarOrder.h ../../include/Proof.h ../../include/Global.h ../../include/File.h ../../include/Heap.h ../../include/Sort.h 
../../include/sat.h: sat.h
	@rm -f ../../include/sat.h
	@ln -fs ../src/sat/sat.h ../../include/sat.h
../../include/Solver.h: Solver.h
	@rm -f ../../include/Solver.h
	@ln -fs ../src/sat/Solver.h ../../include/Solver.h
../../include/SolverTypes.h: SolverTypes.h
	@rm -f ../../include/SolverTypes.h
	@ln -fs ../src/sat/SolverTypes.h ../../include/SolverTypes.h
../../include/VarOrder.h: VarOrder.h
	@rm -f ../../include/VarOrder.h
	@ln -fs ../src/sat/VarOrder.h ../../include/VarOrder.h
../../include/Proof.h: Proof.h
	@rm -f ../../include/Proof.h
	@ln -fs ../src/sat/Proof.h ../../include/Proof.h
../../include/Global.h: Global.h
	@rm -f ../../include/Global.h
	@ln -fs ../src/sat/Global.h ../../include/Global.h
../../include/File.h: File.h
	@rm -f ../../include/File.h
	@ln -fs ../src/sat/File.h ../../include/File.h
../../include/Heap.h: Heap.h
	@rm -f ../../include/Heap.h
	@ln -fs ../src/sat/Heap.h ../../include/Heap.h
../../include/Sort.h: Sort.h
	@rm -f ../../include/Sort.h
	@ln -fs ../src/sat/Sort.h ../../include/Sort.h
