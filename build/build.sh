#!/bin/sh

BUILD_PATH=`pwd`
CURRENT_PATH="${BUILD_PATH}/.."
SRC_PATH="${CURRENT_PATH}/src"
COMPILE_PATH="${CURRENT_PATH}/compile"
INSTALL_PATH="${CURRENT_PATH}/install"

PATH=$PATH:"${INSTALL_PATH}/cmake_binaries/gcc_ubuntu18/bin"

. ./build_common.sh

#clean

create_folders

install_cmake_binaries

build && failed

install

