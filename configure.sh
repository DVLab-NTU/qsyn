#!/bin/bash

TMP_NAME="tmp.log"
ENGINE_INSTALL_PATH="/usr/local"

# Detect OS
# https://stackoverflow.com/questions/394230/how-to-detect-the-os-from-a-bash-script
UNAME_S=$(uname -s)
OSFLAG=""
LINUX_FLAG="-DLINUX"
OSX_FLAG="-DOSX"
if [[ "$UNAME_S" == "Linux"* ]]; then
        OSFLAG+="-DLINUX"
elif [[ "$UNAME_S" == "Darwin"* ]]; then
        OSFLAG+="-DOSX"
        # Mac OSX
# elif [[ "$UNAME_S" == "cygwin" ]]; then
#         # POSIX compatibility layer and Linux environment emulation for Windows
# elif [[ "$UNAME_S" == "msys" ]]; then
#         # Lightweight shell and GNU utilities compiled for Windows (part of MinGW)
# elif [[ "$UNAME_S" == "win32" ]]; then
#         # I'm not sure this can happen.
# elif [[ "$UNAME_S" == "freebsd"* ]]; then
#         # ...
else
        echo -e "Error:\tUnknown OS type.\n"
        exit 1
fi

LIBRARIES=""
if [[ "$OSFLAG" == *"$LINUX_FLAG"* ]]; then
    LIBRARIES+="lapack blas" # dependencies for xtensor-blas
fi

ENGINES="xtl xtensor xtensor-blas"
# ENGINES+=" nlohmann_json zarray"


echo "Installing 3rd-party libraries into ${ENGINE_INSTALL_PATH}..."
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

for library in $LIBRARIES; do
    echo "> Checking vendor/engine/$library..."
    if [ ! -f ${ENGINE_INSTALL_PATH}/lib/lib${library}.a ]; then
        echo "    - Engine $library not found; installing at: ${ENGINE_INSTALL_PATH}/lib"
        ./${library}.sh ${ENGINE_INSTALL_PATH}
    else 
        echo "    - Engine $library already installed at: ${ENGINE_INSTALL_PATH}/lib"
    fi
done

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
cd ..