tensor.d: ../../include/tensor.h 
../../include/tensor.h: tensor.h
	@rm -f ../../include/tensor.h
	@ln -fs ../src/tensor/tensor.h ../../include/tensor.h
