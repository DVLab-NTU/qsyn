ENGINE_NAME="xtensor"
VENDOR_PATH="engine/$ENGINE_NAME"
GITHUB_PATH="https://github.com/xtensor-stack/xtensor.git"

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
make
sudo make install