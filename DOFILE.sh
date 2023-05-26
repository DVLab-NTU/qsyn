#!/bin/bash
source benchmark/qsyn_testing_code/BASH_UTIL.sh

RETURN_CODE=0

function USAGE () {
    printf "%s\t%-12s%-17s%-16s%-9s%s\n" "Usage:" "./DOFILE.sh" "<path/to/dofile>" "[-Diff|-UPdate]" "[-Quiet]" 
    printf "\t%-9s%-2s%-12s\n" "-Diff" ":" "Compare the result to the reference"
    printf "\t%-9s%-2s%-12s\n" "-UPdate" ":" "Update the reference file"
    printf "\t%-9s%-2s%-12s\n" "" "" "If neither -Diff or -UPdate is specified, default to -Diff mode."
    printf "\t%-9s%-2s%-12s\n" "-Quiet " ":" "Don't print diff message"
    printf "\t%-9s%-2s%-12s\n" "-Help" ":" "Print this help message"
    exit 1
} >&2


if (( $# < 1 || $# > 4 )); then 
    USAGE
fi

DODIFF=0
DOUPDATE=0
DOQUIET=0
DOHELP=0

for TOKEN in "$@"; do
    if [[ $(MATCH_STR "$TOKEN" "-help" 2) == 1 ]]; then 
        USAGE
    fi
done

for TOKEN in "$@"; do
    if [[ $(MATCH_STR "$TOKEN" "-diff" 2) == 1 ]]; then
        if [[ $DOUPDATE == 1 ]]; then
            printf "Error: Cannot use -Diff and -UPdate at the same time!!\n"
            exit 1
        fi 
        if [[ $DODIFF == 1 ]]; then
            printf "Error: extra option (%s)!!\n" $TOKEN
            exit 1
        fi 
        DODIFF=1
    elif [[ $(MATCH_STR "$TOKEN" "-update" 3) == 1 ]]; then
        if [[ $DODIFF == 1 ]]; then
            echo "Error: Cannot use -Diff and -UPdate at the same time!!"
            exit 1
        fi 
        if [[ $DOUPDATE == 1 ]]; then
            printf "Error: Extra Option (%s)!!\n" $TOKEN
            exit 1
        fi 
        DOUPDATE=1
    elif [[ $(MATCH_STR "$TOKEN" "-quiet" 2) == 1 ]]; then
        if [[ $DOQUIET == 1 ]]; then
            printf "Error: Extra Option (%s)!!\n" $TOKEN
            exit 1
        fi 
        DOQUIET=1
    elif [ -f $TOKEN ]; then
        FILE_PATH=$TOKEN
    else 
        printf "Error: invalid option!! (%s)\n" $TOKEN
        exit 1
    fi
done

if [[ $DODIFF == 0 && $DOUPDATE == 0 ]]; then DODIFF=1; fi

DOFILE=$(basename $FILE_PATH)
DOFILE_DIR=$(dirname $FILE_PATH)
PKG_NAME=$(dirname $DOFILE_DIR)
REF_FILE=${PKG_NAME}/ref/${DOFILE%%.*}.log
PASS_TEXT="Passed"
FAIL_TEXT="Failed"
DIFF_HEAD="------------------- Differences --------------------"
DIFF_FOOT="----------------------------------------------------"

PASS_TEXT="${BOLD}${GREEN}${PASS_TEXT}${WHITE}${NORMAL}"
FAIL_TEXT="${BOLD}${RED}${FAIL_TEXT}${WHITE}${NORMAL}"
DIFF_HEAD="${BOLD}${RED}${DIFF_HEAD}${WHITE}${NORMAL}"
DIFF_FOOT="${BOLD}${RED}${DIFF_FOOT}${WHITE}${NORMAL}"

if [[ $DODIFF == 1 ]]; then
    printf "Testing %-30s" ${DOFILE}
    OUTPUT=$(./qsyn -f ${FILE_PATH} 2>&1 | diff ${REF_FILE} - )
    STATUS=$?
    if [[ $STATUS == 0 ]]; then 
        echo -e ${PASS_TEXT};
    else 
        echo -e ${FAIL_TEXT}
         RETURN_CODE=1
        if [[ $DOQUIET == 0 ]]; then
            echo -e "${DIFF_HEAD}"
            echo -e "${OUTPUT}"
            echo -e "${DIFF_FOOT}"
        fi
    fi
elif [[ $DOUPDATE == 1 ]]; then
    printf "Updating %s\n" $(basename ${REF_FILE})
    ./qsyn -f ${FILE_PATH} > ${REF_FILE} 2>&1
else 
    echo -e "Error"
    USAGE
fi
exit ${RETURN_CODE}
