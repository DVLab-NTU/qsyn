#!/bin/bash
source benchmark/qsyn_testing_code/BASH_UTIL.sh

echo "Warning: this shell script is deprecated and is to be removed in the near future."
echo "         Please run './RUN_TEST -u' to update all tests instead."

RETURN_CODE=0

function USAGE () {
    printf "%s\t%s\n" "Usage:" "./UPDATE_ALL_TEST.sh" 
    printf "\t%-9s%-2s%-12s\n" "-Help" ":" "Print this help message"
    exit 1
} >&2

if (( $# > 2 )); then 
    USAGE
fi

for TOKEN in "$@"; do
    if [[ $(MATCH_STR "$TOKEN" "-help" 2) == 1 ]]; then 
        USAGE
    else 
        printf "Error: illegal option (%s)!!\n" $TOKEN
        exit 1
    fi
done
TAGS="-up -q"
DOCOLOR=1


echo "> Updating all dofiles' references..."
ALL_TEST_INTERNAL $TAGS
STATUS=$?
if [ $STATUS -ne 0 ]; then 
    RETURN_CODE=1;
fi

echo "Warning: this shell script is deprecated and is to be removed in the near future."
echo "         Please run './RUN_TEST -u' to update all tests instead."

exit ${RETURN_CODE}