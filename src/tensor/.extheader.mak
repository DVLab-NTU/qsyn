tensor.d: ../../include/qtensor.h ../../include/tensor.h ../../include/tensorUtil.h 
../../include/qtensor.h: qtensor.h
	@rm -f ../../include/qtensor.h
	@ln -fs ../src/tensor/qtensor.h ../../include/qtensor.h
../../include/tensor.h: tensor.h
	@rm -f ../../include/tensor.h
	@ln -fs ../src/tensor/tensor.h ../../include/tensor.h
../../include/tensorUtil.h: tensorUtil.h
	@rm -f ../../include/tensorUtil.h
	@ln -fs ../src/tensor/tensorUtil.h ../../include/tensorUtil.h
