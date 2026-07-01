# Copyright(c) The Maintainers of Nanvix.
# Licensed under the MIT License.

FROM ubuntu:24.04

ARG INSTALL_LOCATION=/opt/nanvix

# Build tools required to cross-compile the Nanvix library ports (zlib, bzip2,
# openssl, libffi, xz, sqlite, libxml2, libxslt, cpython, ...). The image ships
# only the LLVM toolchain (clang/lld/llvm-ar); the ports drive their upstream
# build systems (make/autotools/perl) and link the guest with `ld.lld`, which
# has a runtime dependency on libxml2 (without it ld.lld cannot start). Without
# these packages, ld.lld fails to load and make/configure/autotools are absent.
RUN set -eux; \
    export DEBIAN_FRONTEND=noninteractive; \
    apt-get update; \
    apt-get install -y --no-install-recommends \
        make binutils libxml2 pkg-config \
        m4 autoconf automake libtool patch \
        bzip2 xz-utils gzip python3 perl file \
        ca-certificates gawk sed diffutils findutils; \
    rm -rf /var/lib/apt/lists/*

COPY docker-install/ ${INSTALL_LOCATION}/

RUN set -eu; \
    if [ -z "$(ls -A "${INSTALL_LOCATION}" 2>/dev/null)" ]; then \
        echo "ERROR: ${INSTALL_LOCATION} is empty. Ensure the 'Prepare Docker context' workflow step ran successfully." >&2; \
        exit 1; \
    fi

ENV PATH="${INSTALL_LOCATION}/bin:${PATH}"
