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
        xsltproc \


sudo apt-get install -y \
        g++-7 \
        g++-8 \
        g++-9 \
        gcc-7 \
        gcc-8 \
        gcc-9 \

echo
echo "========================================"
echo "Setting up compiler infrastructure"
echo "----------------------------------------"
# g++
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-9 150
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-8 100
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-7 50
# gcc
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 150
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 100
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 50
# set alternatives
sudo update-alternatives --set g++ /usr/bin/g++-7
sudo update-alternatives --set gcc /usr/bin/gcc-7
sudo update-alternatives --set cc /usr/bin/gcc
sudo update-alternatives --set c++ /usr/bin/g++

if [ -z "${BUILD_TOOL}" ]; then
    export BUILD_TOOL=make
fi

echo "----------------------------------------"
