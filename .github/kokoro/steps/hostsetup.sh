#!/bin/bash

set -e

echo
echo "========================================"
echo "Removing older packages"
echo "----------------------------------------"
echo "----------------------------------------"

echo
echo "========================================"
echo "Host adding PPAs"
echo "----------------------------------------"
sudo apt-add-repository 'ppa:ubuntu-toolchain-r/test'
echo "----------------------------------------"

echo
echo "========================================"
echo "Host updating packages"
echo "----------------------------------------"
sudo apt-get update
echo "----------------------------------------"

echo
echo "========================================"
echo "Host remove packages"
echo "----------------------------------------"
sudo apt-get remove -y \
	python-pytest \


sudo apt-get autoremove -y

echo "----------------------------------------"
echo
echo "========================================"
echo "Host install packages"
echo "----------------------------------------"
sudo apt-get install -y \
        bash \
        build-essential \
        cmake \
        coreutils \
        git \
        make \
        tclsh \

if [ -z "${BUILD_TOOL}" ]; then
    export BUILD_TOOL=make
fi

export CC=gcc-7
export CXX=g++-7

echo "----------------------------------------"

echo
echo "========================================"
echo "Setting up environment env"
echo "----------------------------------------"
echo "----------------------------------------"
