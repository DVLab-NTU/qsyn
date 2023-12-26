FROM dvlab/qsyn-env:latest

RUN dnf install -y \
    diffutils \
    patch \
    parallel

COPY ./docker/test-entrypoint.sh /app/entrypoint.sh

WORKDIR /app

ENV CC=/usr/bin/clang
ENV CXX=/usr/bin/clang++

ENTRYPOINT ["/app/entrypoint.sh"]
