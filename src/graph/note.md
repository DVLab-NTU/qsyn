// Change ZXMode
ZXMode [-ON | -OFF | -Reset | -Print]

// New ZX-graph to ZXGraphMgr
ZXNew [(size_t id)]

// Remove ZX-graph from ZXGraphMgr
ZXRemove <(size_t id)>

// Checkout to Graph <id> in ZXGraphMgr
ZXCHeckout <(size_t id)>

// Print info in ZXGraphMgr
ZXPrint [-Summary | -Focus | -Num]

// Copy a ZX-graph
ZXCOPy [(size_t id)]

// Compose a ZX-graph
ZXCOMpose <(size_t id)>

// Tensor a ZX-graph
ZXTensor <(size_t id)>

// Some testing for ZX-graph
ZXGTest [-GenerateCNOT | -Empty | -Valid]

// Print ZX-graph
ZXGPrint [-Summary | -Inputs | -Outputs | -Vertices | -Edges]

// Edit ZX-graph
ZXGEdit -RMVertex [i | <(size_t id(s))> ]
        -RMEdge <(size_t id_s), (size_t id_t)>
        -ADDVertex <(size_t id), (size_t qubit), (VertexType vt)> 
        -ADDInput <(size_t id), (size_t qubit), (VertexType vt)> 
        -ADDOutput <(size_t id), (size_t qubit), (VertexType vt)>
        -ADDEdge <(size_t id_s), (size_t id_t), (EdgeType et)>      
