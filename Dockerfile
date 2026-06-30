# Copyright(c) The Maintainers of Nanvix.
# Licensed under the MIT License.

FROM ubuntu:24.04

ARG INSTALL_LOCATION=/opt/nanvix

COPY docker-install/ ${INSTALL_LOCATION}/

RUN set -eu; \
    if [ -z "$(ls -A "${INSTALL_LOCATION}" 2>/dev/null)" ]; then \
        echo "ERROR: ${INSTALL_LOCATION} is empty. Ensure the 'Prepare Docker context' workflow step ran successfully." >&2; \
        exit 1; \
    fi

ENV PATH="${INSTALL_LOCATION}/bin:${PATH}"
