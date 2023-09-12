FROM dvlab/qsyn-env:latest as builder

COPY . /app/qsyn

WORKDIR /app

RUN mkdir build && cd build && cmake -DCMAKE_CXX_COMPILER=/usr/bin/clang++ ../qsyn && make -j8

FROM fedora:38 AS runner

RUN dnf install -y \
    lapack  \
    openblas-serial  \
    libomp

COPY --from=builder /app/build/qsyn /usr/local/bin/qsyn

ENTRYPOINT ["/usr/local/bin/qsyn"]
