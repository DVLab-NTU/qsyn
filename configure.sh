#!/bin/bash

if [[ $UID != 0 ]]; then
    echo "Please run this script with sudo:"
    echo "sudo $0 $*"
    exit 1
fi

TMP_NAME="tmp.log"
ENGINE_INSTALL_PATH="/usr/local"

ENGINES="xtl xtensor xtensor-blas"
# When appending more engines, please concat this strings. Dependencies should be placed before the package that depends on them. e.g.,
# ENGINES+="dependency_of_another_engine another_engine"

cd vendor
echo | gcc -Wp,-v -x c++ - -fsyntax-only 2> ${TMP_NAME}

# $((...)) performs addition/subtraction
# $(expr ...) turns the strings into ints
# $(awk '/<string>/ {print FNR}' <filename>) get the line number <string> is at in <filename>
GXX_INCLUDE_START=$(($(expr $(awk '/#include <...> search starts here:/ {print FNR}' $TMP_NAME))+1))
GXX_INCLUDE_END=$(($(expr $(awk '/End of search list./ {print FNR}' ${TMP_NAME}))-1))

GXX_INCLUDE_PATHS=$(sed -E 's/^\s+//' ${TMP_NAME} | sed -n ${GXX_INCLUDE_START},${GXX_INCLUDE_END}p)
echo "$GXX_INCLUDE_PATHS" | grep -q "${ENGINE_INSTALL_PATH}/include"
status=$?
# echo "$ENGINE_INSTALL_PATH"
if [ $status -ne 0 ]; then 
    echo "[ERROR] Invalid engine install path.";
    exit 1; 
fi

for engine in $ENGINES; do
    echo "> Checking vendor/engine/$engine..."
    if [ ! -d ${ENGINE_INSTALL_PATH}/include/$engine ]; then 
        echo "    - Engine $engine not found; installing at: ${ENGINE_INSTALL_PATH}/include/$engine"
        ./${engine}.sh ${ENGINE_INSTALL_PATH}
    else 
        echo "    - Engine $engine already installed at: ${ENGINE_INSTALL_PATH}/include/$engine"
    fi
done

rm -f ${TMP_NAME}