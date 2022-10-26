#!/bin/bash
BOLD=$(tput bold)
NORMAL=$(tput sgr0)
RED=$(tput setaf 1)
GREEN=$(tput setaf 2)
WHITE=$(tput sgr0)

N_FILES=$(ls -1q tests/*/*/dof/*.dof | wc -l)

function MATCH_STR () {
    INPUT=$(echo $1 | tr '[:upper:]' '[:lower:]')
    AT_MOST=$2
    AT_LEAST=${AT_MOST:0:$3}

    if  [[ "$AT_MOST" == *"$INPUT"* ]] && \
        [[ "$INPUT" == *"$AT_LEAST"* ]]; then
        echo 1
    else 
        echo 0
    fi
}


function ALL_TEST_INTERNAL () {
    FAIL=0
    for TEST_PKG in tests/*/; do
        DIR_NAME=$(basename $TEST_PKG)
        if [ "$DIR_NAME" = "bin" ]; then continue; fi
        for TEST_SUBPKG in ${TEST_PKG}*/; do
            # Print the package under test
            REL_PATH=$(realpath --relative-to=${PWD}/tests/ ${PWD}/${TEST_SUBPKG})
            printf "> Checking %s...\n" $REL_PATH
            for TEST_FILE in ${TEST_SUBPKG}dof/*; do
                # Test the dofile
                printf "    - "
                ./DOFILE.sh ${TEST_FILE} "$@"
                STATUS=$?
                if [ $STATUS -ne 0 ]; then 
                    RETURN_CODE=1
                    ((FAIL++))
                fi
            done
        done
    done
    return $FAIL
}>&2