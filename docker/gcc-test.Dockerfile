FROM dvlab/qsyn-env:latest

RUN dnf install -y \
    diffutils \
    patch \
    parallel

COPY ./docker/test-entrypoint.sh /app/entrypoint.sh

WORKDIR /app

ENV CC=/usr/bin/gcc
ENV CXX=/usr/bin/g++

ENTRYPOINT ["/app/entrypoint.sh"]
