FROM dvlab/qsyn-env:latest

RUN dnf install -y \
    diffutils \
    patch

COPY ./docker/test-entrypoint.sh /app/entrypoint.sh

WORKDIR /app

ENTRYPOINT ["/app/entrypoint.sh"]
