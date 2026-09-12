# syntax=docker/dockerfile:1
FROM ubuntu@sha256:224a1869083a311ef3f13648a154ba79832fbef6364d31493642ca03082da254

ARG BUILD_ESSENTIAL_VERSION=12.10ubuntu1
ARG CA_CERTIFICATES_VERSION=20260601~24.04.1
ARG CMAKE_VERSION=3.28.3-1build7
ARG CURL_VERSION=8.5.0-2ubuntu10.13
ARG LIBCMOCKA_DEV_VERSION=1.1.7-3
ARG LIBENET_DEV_VERSION=1.3.17+ds-2build1
ARG PKG_CONFIG_VERSION=1.8.1-2build1
ARG VALGRIND_VERSION=1:3.22.0-0ubuntu3
ARG SDL_COMMIT=f48525aa703e0e11134347b38571661d6ca829fd
ARG SDL_SHA256=bb03c0eff839ddccc992d3d0a0f87fbf292ae19b4a7094cac91fd6b7cc9c6e19
ARG SDL_MIXER_COMMIT=3075d3eda55ce295c6919d330edb2554ff4edb5b
ARG SDL_MIXER_SHA256=63c12823e61183e1c5e6f65b0dab365aae5835c50254164f5dcabc8e4a5485ef

RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      build-essential="${BUILD_ESSENTIAL_VERSION}" \
      ca-certificates="${CA_CERTIFICATES_VERSION}" \
      cmake="${CMAKE_VERSION}" \
      curl="${CURL_VERSION}" \
      libcmocka-dev="${LIBCMOCKA_DEV_VERSION}" \
      libenet-dev="${LIBENET_DEV_VERSION}" \
      pkg-config="${PKG_CONFIG_VERSION}" \
      valgrind="${VALGRIND_VERSION}" \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /tmp/deps
RUN set -eu; \
    curl --fail --location --proto '=https' --tlsv1.2 \
      "https://github.com/libsdl-org/SDL/archive/${SDL_COMMIT}.tar.gz" -o SDL.tar.gz; \
    echo "${SDL_SHA256}  SDL.tar.gz" | sha256sum --check -; \
    mkdir SDL; tar -xzf SDL.tar.gz --strip-components=1 -C SDL; \
    cmake -S SDL -B SDL-build \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/valgrind-deps \
      -DCMAKE_INSTALL_LIBDIR=lib64 -DSDL_UNIX_CONSOLE_BUILD=ON \
      -DSDL3_MAINPROJECT=ON -DSDL_INSTALL=ON -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF; \
    cmake --build SDL-build --parallel 2; \
    cmake --install SDL-build; \
    test -f /opt/valgrind-deps/lib64/cmake/SDL3/SDL3Config.cmake; \
    curl --fail --location --proto '=https' --tlsv1.2 \
      "https://github.com/libsdl-org/SDL_mixer/archive/${SDL_MIXER_COMMIT}.tar.gz" -o SDL_mixer.tar.gz; \
    echo "${SDL_MIXER_SHA256}  SDL_mixer.tar.gz" | sha256sum --check -; \
    mkdir SDL_mixer; tar -xzf SDL_mixer.tar.gz --strip-components=1 -C SDL_mixer; \
    cmake -S SDL_mixer -B SDL_mixer-build \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/valgrind-deps \
      -DCMAKE_INSTALL_LIBDIR=lib64 \
      -DSDL3_DIR=/opt/valgrind-deps/lib64/cmake/SDL3 \
      -DSDLMIXER_VENDORED=OFF -DSDLMIXER_EXAMPLES=OFF -DSDLMIXER_TESTS=OFF \
      -DSDLMIXER_FLAC=OFF -DSDLMIXER_GME=OFF -DSDLMIXER_MOD=OFF \
      -DSDLMIXER_MP3=OFF -DSDLMIXER_MIDI=OFF -DSDLMIXER_OPUS=OFF \
      -DSDLMIXER_WAVPACK=OFF; \
    cmake --build SDL_mixer-build --parallel 2; \
    cmake --install SDL_mixer-build; \
    mkdir -p /opt/valgrind-deps/source /opt/valgrind-deps/include /opt/valgrind-deps/lib64; \
    mv SDL /opt/valgrind-deps/source/SDL; \
    cp /usr/include/cmocka.h /opt/valgrind-deps/include/; \
    cp -R /usr/include/enet /opt/valgrind-deps/include/; \
    cp -L /usr/lib/x86_64-linux-gnu/libcmocka.so /opt/valgrind-deps/lib64/libcmocka.so; \
    cp -L /usr/lib/x86_64-linux-gnu/libenet.so /opt/valgrind-deps/lib64/libenet.so; \
    rm -rf /tmp/deps

COPY classify.sh run.sh self-test.sh /usr/local/lib/valgrind-gate/
RUN chmod 0555 /usr/local/lib/valgrind-gate/*.sh \
 && printf '%s\n' \
      "ubuntu_image=ubuntu@sha256:224a1869083a311ef3f13648a154ba79832fbef6364d31493642ca03082da254" \
      "gcc=$(gcc -dumpfullversion)" "glibc=$(ldd --version | head -n 1)" \
      "valgrind=$(valgrind --version)" "cmocka=1.1.7-3" "enet=1.3.17+ds-2build1" \
      "sdl_commit=${SDL_COMMIT}" "sdl_sha256=${SDL_SHA256}" \
      "sdl_mixer_commit=${SDL_MIXER_COMMIT}" "sdl_mixer_sha256=${SDL_MIXER_SHA256}" \
      > /opt/valgrind-deps/manifest.txt

ENV LD_LIBRARY_PATH=/opt/valgrind-deps/lib64
ENTRYPOINT ["/usr/local/lib/valgrind-gate/run.sh"]