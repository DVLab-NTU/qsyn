/****************************************************************************
  FileName     [ zxCmd.h ]
  PackageName  [ graph ]
  Synopsis     [ Define zx package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef ZX_CMD_H
#define ZX_CMD_H

#include "cmdParser.h"   // for CmdExecStatus::CMD_EXEC_ERROR, CmdClass, Cmd...
#include "phase.h"       // for Phase
#include "zxDef.h"       // for VertexType, EdgeType, EdgeType::ERRORTYPE
#include "zxGraph.h"     // for ZXGraph
#include "zxGraphMgr.h"  // for ZXGraphMgr

CmdClass(ZXNewCmd);
CmdClass(ZXResetCmd);
CmdClass(ZXDeleteCmd);
CmdClass(ZXCHeckoutCmd);
CmdClass(ZXPrintCmd);
CmdClass(ZXCOPyCmd);
CmdClass(ZXCOMposeCmd);
CmdClass(ZXTensorCmd);
CmdClass(ZXGPrintCmd);
CmdClass(ZXGTestCmd);
CmdClass(ZXGEditCmd);
CmdClass(ZXGTraverseCmd);
CmdClass(ZXGDrawCmd);
CmdClass(ZX2TSCmd);
CmdClass(ZXGReadCmd);
CmdClass(ZXGWriteCmd);
CmdClass(ZXGAdjointCmd);
CmdClass(ZXGAssignCmd);

#define ZX_CMD_QUBIT_ID_VALID_OR_RETURN(option, qid)         \
    {                                                        \
        if (!myStr2Int((option), (qid))) {                   \
            cerr << "Error: invalid qubit number!!" << endl; \
            return errorOption(CMD_OPT_ILLEGAL, (option));   \
        }                                                    \
    }

#define ZX_CMD_VERTEX_TYPE_VALID_OR_RETURN(option, vt)                                            \
    {                                                                                             \
        (vt) = VertexType::ERRORTYPE;                                                             \
        if (myStrNCmp("Z", (option), 1) == 0) {                                                   \
            (vt) = VertexType::Z;                                                                 \
        } else if (myStrNCmp("X", (option), 1) == 0) {                                            \
            (vt) = VertexType::X;                                                                 \
        } else if (myStrNCmp("H_BOX", (option), 1) == 0 || myStrNCmp("HBOX", (option), 1) == 0) { \
            (vt) = VertexType::H_BOX;                                                             \
        } else {                                                                                  \
            cerr << "Error: invalid vertex type!!" << endl;                                       \
            return errorOption(CMD_OPT_ILLEGAL, (option));                                        \
        }                                                                                         \
    }

#define ZX_CMD_EDGE_TYPE_VALID_OR_RETURN(option, et)          \
    {                                                         \
        (et) = EdgeType::ERRORTYPE;                           \
        if (myStrNCmp("SIMPLE", (option), 1) == 0) {          \
            (et) = EdgeType::SIMPLE;                          \
        } else if (myStrNCmp("HADAMARD", (option), 1) == 0) { \
            (et) = EdgeType::HADAMARD;                        \
        } else {                                              \
            cerr << "Error: invalid edge type!!" << endl;     \
            return errorOption(CMD_OPT_ILLEGAL, (option));    \
        }                                                     \
    }

#define ZX_CMD_VERTEX_ID_IN_GRAPH_OR_RETURN(id, v)                                             \
    {                                                                                          \
        (v) = zxGraphMgr->getGraph()->findVertexById((id));                                    \
        if (!(v)) {                                                                            \
            cerr << "Error: Cannot find vertex with id " << (id) << " in the graph!!" << endl; \
            return CMD_EXEC_ERROR;                                                             \
        }                                                                                      \
    }

#define ZX_CMD_PHASE_VALID_OR_RETURN(option, phase)               \
    {                                                             \
        (phase) = Phase(0);                                       \
        if (!Phase::fromString(option, phase)) {                  \
            cerr << "Error: not a legal phase!!" << endl;         \
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, option); \
        }                                                         \
    }

#define ZX_CMD_GRAPH_ID_NOT_EXIST_OR_RETURN(id)                                                                   \
    {                                                                                                             \
        if (zxGraphMgr->isID(id)) {                                                                               \
            cerr << "Error: Graph " << (id) << " already exists!! Add `-Replace` if you want to overwrite it.\n"; \
            return CMD_EXEC_ERROR;                                                                                \
        }                                                                                                         \
    }

#define ZX_CMD_GRAPH_ID_EXISTS_OR_RETURN(id)                          \
    {                                                                 \
        if (!(zxGraphMgr->isID(id))) {                                \
            cerr << "Error: Graph " << (id) << " does not exist!!\n"; \
            return CMD_EXEC_ERROR;                                    \
        }                                                             \
    }

#define ZX_CMD_GRAPHMGR_NOT_EMPTY_OR_RETURN(str)                                               \
    {                                                                                          \
        if (zxGraphMgr->getgListItr() == zxGraphMgr->getGraphList().end()) {                   \
            cerr << "Error: ZX-graph list is empty now. Please ZXNew before " << str << ".\n"; \
            return CMD_EXEC_ERROR;                                                             \
        }                                                                                      \
    }

#define ZX_CMD_ID_VALID_OR_RETURN(option, id, str)         \
    {                                                      \
        if (!myStr2Uns((option), (id))) {                  \
            cerr << "Error: invalid " << str << " ID!!\n"; \
            return errorOption(CMD_OPT_ILLEGAL, (option)); \
        }                                                  \
    }

#endif  // ZX_CMD_H