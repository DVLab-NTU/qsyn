FROM fedora:38

RUN dnf -y install \
    git \
    clang \
    cmake \
    lapack-devel  \
    openblas-devel \
    diffutils \
    patch

COPY ./entrypoint.sh /app/entrypoint.sh
WORKDIR /app
ENTRYPOINT ["/app/entrypoint.sh"]
