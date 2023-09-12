FROM qsyn-env

COPY . /app/qsyn

WORKDIR /app

RUN mkdir build && cd build && cmake ../qsyn && make -j8

# FROM fedora:38 AS runner
#
# COPY --from=builder /app/qsyn/build/qsyn /usr/local/bin/qsyn
#
# ENTRYPOINT ["/usr/local/bin/qsyn"]
