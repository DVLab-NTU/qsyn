FROM qsyn-env

RUN dnf -y install \
    diffutils \
    patch

COPY ./entrypoint.sh /app/entrypoint.sh

WORKDIR /app

ENTRYPOINT ["/app/entrypoint.sh"]
