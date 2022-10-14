#!/bin/bash
RETURN_CODE=0

echo "> Testing commands..."
for TEST_PKG in tests/*/; do
    DIR_NAME=$(basename $TEST_PKG)
    if [ "$DIR_NAME" = "bin" ]; then continue; fi
    echo $DIR_NAME
    for TEST_SUBPKG in ${TEST_PKG}*/; do
        # Print the package under test
        REL_PATH=$(realpath --relative-to=${PWD}/tests/ ${PWD}/${TEST_SUBPKG})
        printf "> Checking %s...\n" $REL_PATH
        for TEST_FILE in ${TEST_SUBPKG}dof/*; do
            # Test the dofile
            printf "    - "
            ./DOFILE.sh ${TEST_FILE} -d
            STATUS=$?
            if [ $STATUS -ne 0 ]; then 
                RETURN_CODE=1;
            fi
        done
    done
done

printf "\n> Testing functions...\n"
./tests/bin/tests -r compact
STATUS=$?
if [ $STATUS -ne 0 ]; then 
    RETURN_CODE=1;
fi

exit ${RETURN_CODE}