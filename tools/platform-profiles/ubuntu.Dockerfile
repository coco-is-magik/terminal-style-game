FROM ubuntu@sha256:224a1869083a311ef3f13648a154ba79832fbef6364d31493642ca03082da254

ARG PROFILE_ID
ARG PROFILE_ROLE
ARG PROFILE_COMPILER
ARG PROFILE_COMPILER_PACKAGE
ARG SDL_COMMIT
ARG SDL_SHA256
ARG SDL_MIXER_COMMIT
ARG SDL_MIXER_SHA256
ARG ENET_COMMIT
ARG ENET_SHA256
ARG CMOCKA_COMMIT
ARG CMOCKA_SHA256
ARG SMC_COMMIT
ARG SMC_SHA256

RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      build-essential ca-certificates cmake curl pkg-config ${PROFILE_COMPILER_PACKAGE} \
 && rm -rf /var/lib/apt/lists/*

COPY build-dependencies.sh /usr/local/lib/platform-profile/build-dependencies.sh
RUN chmod 0555 /usr/local/lib/platform-profile/build-dependencies.sh \
 && CC="${PROFILE_COMPILER}" /usr/local/lib/platform-profile/build-dependencies.sh

COPY classify.sh run-profile.sh /usr/local/lib/platform-profile/
RUN chmod 0555 /usr/local/lib/platform-profile/*.sh \
 && dpkg-query -W > /opt/platform-deps/packages.txt

ENV PROFILE_ID=${PROFILE_ID} PROFILE_ROLE=${PROFILE_ROLE} PROFILE_COMPILER=${PROFILE_COMPILER}
ENV SDL_COMMIT=${SDL_COMMIT} SDL_MIXER_COMMIT=${SDL_MIXER_COMMIT} ENET_COMMIT=${ENET_COMMIT}
ENV CMOCKA_COMMIT=${CMOCKA_COMMIT} SMC_COMMIT=${SMC_COMMIT}
ENV LD_LIBRARY_PATH=/opt/platform-deps/lib64
ENTRYPOINT ["/usr/local/lib/platform-profile/run-profile.sh"]