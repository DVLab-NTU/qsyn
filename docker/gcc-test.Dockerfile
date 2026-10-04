FROM dvlab/qsyn-env:latest

# Install required tools + pip
RUN apt-get update && \
    apt-get install -y diffutils patch parallel python3-pip && \
    python3 -m pip install --upgrade pip --break-system-packages && \
    python3 -m pip install "cmake==3.29.6" --break-system-packages && \
    python3 -m pip install "ninja==1.11.1" --break-system-packages

COPY ./docker/test-entrypoint.sh /app/entrypoint.sh

WORKDIR /app

ENV CC=/usr/bin/gcc
ENV CXX=/usr/bin/g++

ENTRYPOINT ["/app/entrypoint.sh"]
