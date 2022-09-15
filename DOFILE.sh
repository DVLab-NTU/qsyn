#!/bin/bash
USAGE_STR="Usage:\t./RUN_DOFILE.sh <-Diff|-Update> <path/to/dofile>\n\t-d: Compare the result to the reference\n\t-u: Update the reference file\n"
return_code=0
if [ $# -ne 2 ]; then 
    echo -e ${USAGE_STR}
    exit 1; 
fi

FILE_PATH=$2
if [ ! -f $FILE_PATH ]; then 
    echo -e "Error\tInvalid dofile path!"
    exit -1
fi

DOFILE=$(basename $FILE_PATH)
DOFILE_DIR=$(dirname $FILE_PATH)
PKG_NAME=$(dirname $DOFILE_DIR)
REF_FILE=${PKG_NAME}/reference/${DOFILE}

MODE_ARG=$(echo $1 | awk '{print tolower($0)}')
DIFF_STR="-diff"
DIFF_STR_AT_LEAST="-d"
UPDATE_STR="-update"
UPDATE_STR_AT_LEAST="-u"

bold=$(tput bold)
normal=$(tput sgr0)
red=$(tput setaf 1)
green=$(tput setaf 2)
white=$(tput sgr0)
pass_text="${bold}${green}Passed${white}${normal}"
fail_text="${bold}${red}Failed\n--- Differences than the reference: ---${white}${normal}"

if [[ "$DIFF_STR" == *"$MODE_ARG"* ]] && \
   [[ "$MODE_ARG" == *"$DIFF_STR_AT_LEAST"* ]]; then
    printf "Testing %s/.../%-20s" ${PKG_NAME} ${DOFILE}
    OUTPUT=$(./qsyn -f ${FILE_PATH} 2>/dev/null | diff ${REF_FILE} -)
    status=$?
    if [ $status -eq 0 ]; then 
        echo ${pass_text};
    else 
        echo -e ${fail_text}; return_code=1;
        echo -e "$OUTPUT"
    fi
elif [[ "$UPDATE_STR" == *"$MODE_ARG"* ]] && \
        [[ "$MODE_ARG" == *"$UPDATE_STR_AT_LEAST"* ]]; then
    printf "Note:\tOverwriting %s...\n" ${REF_FILE}
    ./qsyn -f ${FILE_PATH} 2>/dev/null 1> ${REF_FILE}
else 
    echo -e "Error:\tInvalid options!"
    echo -e ${USAGE_STR}
    exit -1; 
fi

exit ${return_code}