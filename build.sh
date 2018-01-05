#!/bin/bash

function prepare() {
  apt-get -y install qemu bridge-utils uml-utilities libc_i386 libexpat1-dev libncurses5-dev
}


function download_sylixos() {
  if [ -e arm-none-eabi ]
  then
    echo sylixos exist
  else
    mkdir sylixos
    cd sylixos
    git clone http://git.sylixos.com/repo/sylixos-base.git
    git clone http://git.sylixos.com/repo/bspmini2440.git
    git clone http://git.sylixos.com/repo/examples.git
    git clone http://git.sylixos.com/repo/tools.git
    git clone http://git.sylixos.com/repo/qemu-mini2440.git
    cd sylixos-base
    git submodule init
    git submodule update
  fi
}

function link_arm_sylixos_eabi() {
  cd arm-none-eabi/bin
  for f in arm-none-eabi-* 
  do
    ln -s $f ${f/none/sylixos}
  done
  cd -
}

function download_toolchain() {
  NAME=gcc-arm-none-eabi-5_4-2016q3-20160926-linux

  if [ -e ${NAME}.tar.bz2 ]
  then 
    echo ${NAME}.tar.bz2 exist
  else 
    wget https://launchpad.net/gcc-arm-embedded/5.0/5-2016-q3-update/+download/${NAME}.tar.bz2
  fi

  if [ -e arm-none-eabi ]
  then
    echo arm-none-eabi exist
  else
    tar xf ${NAME}.tar.bz2
    mv gcc-arm-none-eabi-5_4-2016q3 arm-none-eabi
    cp -fv sylixos/tools/arm-none-eabi-patch/4.9/reent.h arm-none-eabi/arm-none-eabi/include/sys/reent.h

    link_arm_sylixos_eabi
  fi
}

export PATH=$PATH:${PWD}/arm-none-eabi/bin
export SYLIXOS_BASE_PATH=${PWD}/sylixos/sylixos-base

function build_sylixos() {
  cd sylixos/sylixos-base
  make
  cd -

  cd sylixos/bspmini2440
  make SYLIXOS_BASE_PATH=${SYLIXOS_BASE_PATH}
  cd -

  cd sylixos/examples
  make SYLIXOS_BASE_PATH=${SYLIXOS_BASE_PATH}
  cd -
}

function build() {
  prepare
  download_sylixos
  download_toolchain
  build_sylixos
}

build

