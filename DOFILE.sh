#!/bin/bash
USAGE_STR="Usage:\t./RUN_DOFILE.sh <path/to/dofile> <-Diff|-UPdate>\n\t-d : Compare the result to the reference\n\t-up: Update the reference file\n"
RETURN_CODE=0
if [ $# -ne 2 ]; then 
    echo -e ${USAGE_STR}
    exit 1; 
fi

FILE_PATH=$1
if [ ! -f $FILE_PATH ]; then 
    echo -e "Error\tInvalid dofile path!"
    exit -1
fi

DOFILE=$(basename $FILE_PATH)
DOFILE_DIR=$(dirname $FILE_PATH)
PKG_NAME=$(dirname $DOFILE_DIR)
REF_FILE=${PKG_NAME}/ref/${DOFILE%%.*}.log

MODE_ARG=$(echo $2 | awk '{print tolower($0)}')
DIFF_STR="-diff"
DIFF_STR_AT_LEAST="-d"
UPDATE_STR="-update"
UPDATE_STR_AT_LEAST="-up"

BOLD=$(tput bold)
NORMAL=$(tput sgr0)
RED=$(tput setaf 1)
GREEN=$(tput setaf 2)
WHITE=$(tput sgr0)
PASS_TEXT="${BOLD}${GREEN}Passed${WHITE}${NORMAL}"
FAIL_TEXT="${BOLD}${RED}Failed${WHITE}${NORMAL}"

if [[ "$DIFF_STR" == *"$MODE_ARG"* ]] && \
   [[ "$MODE_ARG" == *"$DIFF_STR_AT_LEAST"* ]]; then
    printf "Testing %-30s" ${DOFILE}
    ./qsyn -f ${FILE_PATH} |& diff ${REF_FILE} - > /dev/null 2>&1 
    STATUS=$?
    if [ $STATUS -eq 0 ]; then 
        echo ${PASS_TEXT};
    else 
        echo -e ${FAIL_TEXT}; RETURN_CODE=1;
    fi
elif [[ "$UPDATE_STR" == *"$MODE_ARG"* ]] && \
        [[ "$MODE_ARG" == *"$UPDATE_STR_AT_LEAST"* ]]; then
    printf "Updating %s\n" $(basename ${REF_FILE})
    ./qsyn -f ${FILE_PATH} > ${REF_FILE} 2>&1
else 
    echo -e "Error:\tInvalid options!"
    echo -e ${USAGE_STR}
    exit -1; 
fi

exit ${RETURN_CODE}