graph.d: ../../include/zxGraphMgr.h ../../include/zxGraph.h ../../include/zxDef.h ../../include/zxCmd.h 
../../include/zxGraphMgr.h: zxGraphMgr.h
	@rm -f ../../include/zxGraphMgr.h
	@ln -fs ../src/graph/zxGraphMgr.h ../../include/zxGraphMgr.h
../../include/zxGraph.h: zxGraph.h
	@rm -f ../../include/zxGraph.h
	@ln -fs ../src/graph/zxGraph.h ../../include/zxGraph.h
../../include/zxDef.h: zxDef.h
	@rm -f ../../include/zxDef.h
	@ln -fs ../src/graph/zxDef.h ../../include/zxDef.h
../../include/zxCmd.h: zxCmd.h
	@rm -f ../../include/zxCmd.h
	@ln -fs ../src/graph/zxCmd.h ../../include/zxCmd.h
