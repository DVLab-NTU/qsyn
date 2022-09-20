graph.d: ../../include/zxGraphMgr.h ../../include/zxGraph.h 
../../include/zxGraphMgr.h: zxGraphMgr.h
	@rm -f ../../include/zxGraphMgr.h
	@ln -fs ../src/graph/zxGraphMgr.h ../../include/zxGraphMgr.h
../../include/zxGraph.h: zxGraph.h
	@rm -f ../../include/zxGraph.h
	@ln -fs ../src/graph/zxGraph.h ../../include/zxGraph.h
