ENGINE_NAME="BLAS-3.11.0"
VENDOR_PATH=$(mktemp -d)
DOWNLOAD_PATH="http://www.netlib.org/blas/blas-3.11.0.tgz"
JOB=16
if [ $# -ne 1 ]; then 
    echo "[Error] Missing install path!"
    exit 1
fi

INSTALL_PATH=$1
echo $VENDOR_PATH
mkdir -p ${VENDOR_PATH}
rm -rf ${VENDOR_PATH}/${ENGINE_NAME}
cd ${VENDOR_PATH}
curl -L ${DOWNLOAD_PATH} --output ${ENGINE_NAME}.tgz
tar -xf ${ENGINE_NAME}.tgz

cd ${ENGINE_NAME}
make -j${JOB}
sudo mv blas_LINUX.a libblas.a
sudo cp *.a ${INSTALL_PATH}/lib
