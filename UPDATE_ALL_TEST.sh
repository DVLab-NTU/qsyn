#!/bin/bash
source qsyn_testing_code/BASH_UTIL.sh

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
# if [[ $DOCOLOR == 1 ]]; then 
#     if [[ $STATUS == 0 ]]; then
#         RESULT_TXT="${BOLD}${GREEN}Passed all ${N_FILES} dofiles.${WHITE}${NORMAL}"
#     else 
#         RESULT_TXT="${BOLD}${RED}${STATUS} out of ${N_FILES} dofiles failed.${WHITE}${NORMAL}"
#     fi
# fi
# echo -e $RESULT_TXT

exit ${RETURN_CODE}