FROM alpine@sha256:14358309a308569c32bdc37e2e0e9694be33a9d99e68afb0f5ff33cc1f695dce

ARG PROFILE_ID
ARG PROFILE_ROLE
ARG PROFILE_COMPILER
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

RUN apk add --no-cache build-base cmake curl ca-certificates linux-headers pkgconf

COPY build-dependencies.sh /usr/local/lib/platform-profile/build-dependencies.sh
RUN chmod 0555 /usr/local/lib/platform-profile/build-dependencies.sh \
 && CC="${PROFILE_COMPILER}" /usr/local/lib/platform-profile/build-dependencies.sh

COPY classify.sh run-profile.sh /usr/local/lib/platform-profile/
RUN chmod 0555 /usr/local/lib/platform-profile/*.sh \
 && apk info -vv | sort > /opt/platform-deps/packages.txt

ENV PROFILE_ID=${PROFILE_ID} PROFILE_ROLE=${PROFILE_ROLE} PROFILE_COMPILER=${PROFILE_COMPILER}
ENV SDL_COMMIT=${SDL_COMMIT} SDL_MIXER_COMMIT=${SDL_MIXER_COMMIT} ENET_COMMIT=${ENET_COMMIT}
ENV CMOCKA_COMMIT=${CMOCKA_COMMIT} SMC_COMMIT=${SMC_COMMIT}
ENV LD_LIBRARY_PATH=/opt/platform-deps/lib64
ENTRYPOINT ["/usr/local/lib/platform-profile/run-profile.sh"]