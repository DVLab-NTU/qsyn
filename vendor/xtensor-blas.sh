ENGINE_NAME="xtensor-blas"
VENDOR_PATH="engine/$ENGINE_NAME"
GITHUB_PATH="https://github.com/xtensor-stack/xtensor-blas.git"
JOB=16
if [ $# -ne 1 ]; then 
    echo "[Error] Missing install path!"
    exit 1
fi

INSTALL_PATH=$1

rm -rf ${VENDOR_PATH}
git clone ${GITHUB_PATH} ${VENDOR_PATH}

cd ${VENDOR_PATH}

mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=${INSTALL_PATH}
make -j${JOB}
sudo make install