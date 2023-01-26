/****************************************************************************
  FileName     [ argparseErrorMsg.cpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define error outputs for argparser commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "argparseErrorMsg.h"

#include <iostream>

#include "argparseArgument.h"
#include "textFormat.h"

using namespace std;

namespace TF = TextFormat;

namespace ArgParse {

namespace detail {

void printArgumentCastErrorMsg(Argument const& arg) {
    cerr << "[ArgParse] Error: failed to cast argument!!";
    if (arg.getName().size()) {
        cerr << "\"" << arg.getName() << "\" ";
    }
    cerr << "Only castable to type \"" << arg.getTypeString() << "\"." << endl;
}

void printDefaultValueErrorMsg(Argument const& arg) {
    cerr << "[ArgParse] Error: failed to assign default value to argument \"" 
         << arg.getName() + "\"!!" << endl;
}

void printArgParseFatalErrorMsg() {
    cerr << TF::RED("[ArgParse] Fatal error: cannot recover from ill-formed parsing logic. Exiting program...") << endl;
}


void printArgNameEmptyErrorMsg() {
    cerr << "[ArgParse] Error: Argument name cannot be an empty string!!" << endl;
}

void printArgNameDuplicateErrorMsg(std::string const& name) {
    cerr << "[ArgParse] Error: Argument name \"" << name << "\" is already used by another argument!!" << endl;
}

void printDuplicatedAttrErrorMsg(Argument const& arg, std::string const& attrName) {
    cerr << "[ArgParse] Error: Failed to add attribute \"" << attrName << "\" to argument \"" << arg.getName() << "\": attribute duplicated" << endl;
}

}

}