ENGINE_NAME="BLAS-3.9.0"
VENDOR_PATH="engine/$ENGINE_NAME"
DOWNLOAD_PATH="http://www.netlib.org/blas/blas-3.9.0.tgz"
JOB=16
if [ $# -ne 1 ]; then 
    echo "[Error] Missing install path!"
    exit 1
fi

INSTALL_PATH=$1

rm -rf ${VENDOR_PATH}
cd engine
curl -L ${DOWNLOAD_PATH} --output ${ENGINE_NAME}.tgz
tar -xf ${ENGINE_NAME}.tgz

cd ${ENGINE_NAME}
make -j${JOB}
sudo mv blas_LINUX.a libblas.a
sudo cp *.a ${INSTALL_PATH}/lib
