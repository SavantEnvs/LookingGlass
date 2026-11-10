# syntax=docker/dockerfile:1.7
#
# Commit image for LookingGlass: builds on the org C/C++ base and bakes in the per-commit build
# (mayhem/build.sh) and the functional suite (mayhem/test.sh).
#
#   DOCKER_BUILDKIT=1 docker build -f mayhem/Dockerfile --build-arg COMMIT=$(git rev-parse HEAD) -t mytag .
FROM ghcr.io/savantenvs/base:20261023

ARG COMMIT
ARG REPO_URL
ARG SOURCE_DATE_EPOCH
ARG SANITIZER_FLAGS
ARG COVERAGE_FLAGS=
ARG MAYHEM_JOBS=

# Headers only: the client's config.c includes the EGL/GL/GLES renderer interface headers.
USER root
RUN [ -n "$SOURCE_DATE_EPOCH" ] && repro-sources-list.sh || true ; \
    apt-get update && apt-get install -yq --no-install-recommends \
        libegl-dev libgl-dev libgles-dev \
 && rm -rf /var/lib/apt/lists/*

# Upstream's git submodules (repos/*) are empty in a plain checkout, so the build cannot reach them.
# Pin the exact commits upstream's gitlinks name and cache them under /opt/toolchains/deps
# (SPEC §6.2 item 19). LGProtocol and cimgui are headers only; LGMP is built from src/ by build.sh
# because the LGMP client is itself fuzzed.
ARG LGPROTOCOL_COMMIT=62b52724b63b6e179b8fb7e0c7c03e9a8b4e1636
ARG LGPROTOCOL_SHA256=a164b85e3cd728b23924b5127124fa8721ccb31b52ca5dd7e75b85e5803887db
ARG LGMP_COMMIT=10e3fef41de71d5ecb4fe34e4a6aa721a86df84d
ARG LGMP_SHA256=4156f1793f506e4e93203c378a23ddc3f499d6055077975b3fdf3db625947fbd
ARG CIMGUI_COMMIT=4e8fcc13674481a8c4c04abbfea90915e3896fe7
ARG CIMGUI_SHA256=c18451c2aa88fc8624fe357dc1e0cad88c27e022f2897047aa53b7ad3f6312f4
RUN set -e; \
    fetch() { \
      d="/opt/toolchains/deps/$1-$2"; mkdir -p "$d/src"; \
      curl -fsSL -o /tmp/dep.tgz "https://codeload.github.com/$3/tar.gz/$2"; \
      echo "$4  /tmp/dep.tgz" | sha256sum -c -; \
      tar -xzf /tmp/dep.tgz --strip-components=1 -C "$d/src"; \
      rm -f /tmp/dep.tgz; chmod -R a+rX "$d"; \
    }; \
    fetch LGProtocol "$LGPROTOCOL_COMMIT" gnif/LGProtocol "$LGPROTOCOL_SHA256"; \
    fetch LGMP       "$LGMP_COMMIT"       gnif/LGMP       "$LGMP_SHA256"; \
    fetch cimgui     "$CIMGUI_COMMIT"     cimgui/cimgui   "$CIMGUI_SHA256"

USER mayhem
WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

RUN if [ -x /opt/mayhem/ccache/prefill ]; then /opt/mayhem/ccache/prefill bash mayhem/build.sh; \
    else bash mayhem/build.sh; fi

RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh — repo ships no functional tests"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem"

USER mayhem
WORKDIR /mayhem
