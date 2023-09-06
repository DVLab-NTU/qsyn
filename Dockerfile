FROM fedora:38

RUN dnf -y install \
    git \
    clang \
    cmake \
    lapack-devel  \
    openblas-devel \
    diffutils 

# COPY . /qsyn
# WORKDIR /qsyn
# RUN rm -rf build && mkdir build && cd build && cmake -DCMAKE_CXX_COMPILER=/usr/bin/clang++ .. && make -j6
