#!/bin/sh

failed()
{
    echo "Previous function failed, hence quitting"
    exit 0
}

create_folders()
{
    mkdir -p "${INSTALL_PATH}/cmake_binaries/gcc_ubuntu18"
    mkdir -p "${COMPILE_PATH}/"
    mkdir -p "${INSTALL_PATH}/"
}

install_cmake_binaries()
{
    if [ ! -f "${INSTALL_PATH}/cmake_binaries/gcc_ubuntu18/bin/cmake" ]; then
        echo "---Installing cmake_binaries---"
        mkdir -p "${INSTALL_PATH}/cmake_binaries/gcc_ubuntu18"
        wget -q -O cmake-linux.sh https://github.com/Kitware/CMake/releases/download/v3.17.3/cmake-3.17.3-Linux-x86_64.sh
        sh cmake-linux.sh -- --skip-license --prefix="${INSTALL_PATH}/cmake_binaries/gcc_ubuntu18"
        rm cmake-linux.sh
    fi
}

clean()
{
    echo "---Cleaning binaries---"

    rm -rf ${COMPILE_PATH}
    rm -rf ${INSTALL_PATH}
}

build()
{
    echo "---Building binaries---"
    cd "${COMPILE_PATH}"

    cmake -DCMAKE_BUILD_TYPE=Release "${SRC_PATH}"
    make

    if [ $? -ne 0 ]; then
        # Above make failed
        return 0
    fi

    return 1
}

install()
{
    echo "---Installing binaries---"
    cp -af ${COMPILE_PATH}/server ${INSTALL_PATH}
    cp -af ${COMPILE_PATH}/client ${INSTALL_PATH}
}
