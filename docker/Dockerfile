FROM ubuntu:20.04

ARG DEBIAN_FRONTEND=noninteractive

# OS deps only; toolchains will be built from bind-mounted sources into /work/.tools.
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    git \
    make \
    build-essential \
    pkg-config \
    python3 \
    python3-dev \
    python3-pip \
    python3-setuptools \
    python3-wheel \
    autoconf \
    automake \
    libtool \
    flex \
    bison \
    gperf \
    help2man \
    zlib1g-dev \
    libfl2 \
    libfl-dev \
    libexpat1-dev \
    device-tree-compiler \
    libboost-dev \
    libboost-regex-dev \
    libboost-system-dev \
    gcc-riscv64-unknown-elf \
    binutils-riscv64-unknown-elf \
    ocaml \
    opam \
    ocaml-findlib \
    ocamlbuild \
    z3 \
    libgmp-dev \
 && rm -rf /var/lib/apt/lists/*

ENV RISCV=/opt/riscv
ENV PATH=${RISCV}/bin:${PATH}

WORKDIR /work