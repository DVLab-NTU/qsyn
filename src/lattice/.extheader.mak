lattice.d: ../../include/lattice.h 
../../include/lattice.h: lattice.h
	@rm -f ../../include/lattice.h
	@ln -fs ../src/lattice/lattice.h ../../include/lattice.h
