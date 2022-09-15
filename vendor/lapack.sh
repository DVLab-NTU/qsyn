ENGINE_NAME="lapack-3.9.0"
VENDOR_PATH="engine/$ENGINE_NAME"
DOWNLOAD_PATH="https://github.com/Reference-LAPACK/lapack/archive/v3.9.0.tar.gz"
if [ $# -ne 1 ]; then 
    echo "[Error] Missing install path!"
    exit 1
fi

INSTALL_PATH=$1

rm -rf ${VENDOR_PATH}
cd engine
curl -L ${DOWNLOAD_PATH} --output ${ENGINE_NAME}.tar.gz
tar -xf ${ENGINE_NAME}.tar.gz > /dev/null

cd ${ENGINE_NAME}
cp make.inc.example make.inc
make -j8
sudo cp *.a ${INSTALL_PATH}/lib