FROM ubuntu:20.04 AS base

USER root
WORKDIR /

SHELL ["/bin/bash", "-c"]

ENV DEBIAN_FRONTEND=noninteractive

COPY ./ci-setup.sh ./scripts/ci-setup.sh
COPY ./ci-setup-helper-functions.sh ./scripts/ci-setup-helper-functions.sh

RUN bash ./scripts/ci-setup.sh