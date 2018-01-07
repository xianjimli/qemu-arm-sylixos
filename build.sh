#!/bin/bash

function prepare() {
  apt-get -y install bridge-utils uml-utilities libc6-i386 libexpat1-dev libncurses5-dev axel libglib2.0-dev libpixman-1-dev libfdt-dev libsdl-dev
}

PROJ_ROOT_DIR=$PWD
function download_sylixos() {
  if [ -e sylixos ]
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
    cp -fv patch/lwip_netif.c sylixos/sylixos-base/libsylixos/SylixOS/net/lwip/lwip_netif.c
    cp -fv patch/gcc.mk sylixos/sylixos-base/libsylixos/SylixOS/mktemp/gcc.mk
  fi
  cd $PROJ_ROOT_DIR
}

function link_arm_sylixos_eabi() {
  cd arm-none-eabi/bin
  for f in arm-none-eabi-* 
  do
    ln -s $f ${f/none/sylixos}
  done
  cd $PROJ_ROOT_DIR
}

function download_toolchain() {
  NAME=gcc-arm-none-eabi-5_4-2016q3-20160926-linux

  if [ -e ${NAME}.tar.bz2 ]
  then 
    echo ${NAME}.tar.bz2 exist
  else 
    axel -n 8 https://launchpad.net/gcc-arm-embedded/5.0/5-2016-q3-update/+download/${NAME}.tar.bz2
  fi
  
  if [ ! -e ${NAME}.tar.bz2 ]
  then 
    echo download ${NAME}.tar.bz2 failed.
    exit
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
  cd $PROJ_ROOT_DIR
}

export PATH=$PATH:${PWD}/arm-none-eabi/bin
export SYLIXOS_BASE_PATH=${PWD}/sylixos/sylixos-base

function build_sylixos() {
  cd sylixos/sylixos-base
  make
  cd -

  cd sylixos/bspmini2440
  make SYLIXOS_BASE_PATH=${SYLIXOS_BASE_PATH} all
  cd -

  cd sylixos/examples
  make SYLIXOS_BASE_PATH=${SYLIXOS_BASE_PATH}
  cd -

  cd $PROJ_ROOT_DIR
}

function download_build_qemu_mini2440() {
  if [ ! -e qemu-mini2440 ]
  then 
    git clone https://github.com/ywandy/qemu-mini2440.git
    cd qemu-mini2440 && ./configure --target-list=arm-softmmu && make -j4
    cd $PROJ_ROOT_DIR
    cp -fv ./qemu-mini2440/arm-softmmu/qemu-system-arm .
  fi
}

function prepare_qemu() {
  ./sylixos/qemu-mini2440/nandCreator.c -o nandcreate;./nandcreate

}

function build() {
  prepare
  download_sylixos
  download_toolchain
  build_sylixos
  download_build_qemu_mini2440
}

build

echo "Done"
