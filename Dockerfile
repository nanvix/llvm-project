# Copyright(c) The Maintainers of Nanvix.
# Licensed under the MIT License.

FROM ubuntu:24.04

ARG INSTALL_LOCATION=/opt/nanvix

COPY docker-install/ ${INSTALL_LOCATION}/

ENV PATH="${INSTALL_LOCATION}/bin:${PATH}"
