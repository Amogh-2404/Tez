FROM alpine:3.24@sha256:28bd5fe8b56d1bd048e5babf5b10710ebe0bae67db86916198a6eec434943f8b AS build

RUN apk add --no-cache g++ cmake ninja boost-dev nlohmann-json
WORKDIR /src
COPY CMakeLists.txt ./
COPY include/ include/
COPY src/ src/
COPY config.json LICENSE ./
COPY static/ static/
ARG TEZ_VERSION=1.1.0-dev
RUN cmake -S . -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
        -DTEZ_WARNINGS_AS_ERRORS=ON -DTEZ_VERSION="${TEZ_VERSION}" \
    && cmake --build build --parallel 2 \
    && cmake --install build --prefix /out --strip

FROM alpine:3.24@sha256:28bd5fe8b56d1bd048e5babf5b10710ebe0bae67db86916198a6eec434943f8b
RUN apk add --no-cache libstdc++ \
    && addgroup -S -g 10001 tez \
    && adduser -S -D -H -u 10001 -G tez -s /sbin/nologin tez
ARG TEZ_VERSION=1.1.0-dev
LABEL org.opencontainers.image.title="Tez" \
      org.opencontainers.image.description="A small C++17 HTTP server built on Boost.Asio and Boost.Beast." \
      org.opencontainers.image.url="https://github.com/Amogh-2404/Tez" \
      org.opencontainers.image.source="https://github.com/Amogh-2404/Tez" \
      org.opencontainers.image.licenses="MIT" \
      org.opencontainers.image.authors="R.Amogh" \
      org.opencontainers.image.version="${TEZ_VERSION}"
COPY --from=build /out/bin/Tez /usr/local/bin/Tez
COPY --from=build /out/share/tez/ /app/
COPY --from=build /out/share/licenses/tez/LICENSE /usr/share/licenses/tez/LICENSE
WORKDIR /app
USER 10001:10001
EXPOSE 8080
STOPSIGNAL SIGTERM
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD wget -q -T 2 -O /dev/null http://127.0.0.1:8080/health || exit 1
ENTRYPOINT ["/usr/local/bin/Tez"]
CMD ["--address", "0.0.0.0", "--config", "/app/config.json", "--static-dir", "/app/static"]
