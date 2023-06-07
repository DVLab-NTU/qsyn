ENGINE_NAME="lapack-3.10.1"
VENDOR_PATH=$(mktemp -d)
DOWNLOAD_PATH="https://github.com/Reference-LAPACK/lapack/archive/v3.10.1.tar.gz"
JOB=16
if [ $# -ne 1 ]; then 
    echo "[Error] Missing install path!"
    exit 1
fi

INSTALL_PATH=$1

mkdir -p ${VENDOR_PATH}
rm -rf ${VENDOR_PATH}/${ENGINE_NAME}
cd ${VENDOR_PATH}
curl -L ${DOWNLOAD_PATH} --output ${ENGINE_NAME}.tar.gz
tar -xf $ENGINE_NAME.tar.gz > /dev/null

cd ${ENGINE_NAME}
cp make.inc.example make.inc
make lib -j${JOB}
make cblaslib -j${JOB}
make lapack_install -j${JOB}
sudo cp *.a ${INSTALL_PATH}/lib