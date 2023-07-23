#!/bin/sh
#
# Install the things which need adding to a fresh Fedora or Centos install to make it ready to build
# spandsp and its test suite
#
yum groupinstall "Development tools"
yum install fftw-devel \
            libtiff-devel \
            libtiff-tools \
            libjpeg-turbo-devel \
            libpcap-devel \
            libxml2-devel \
            libsndfile-devel \
            libuv-devel \
            fltk-devel \
            fltk-fluid \
            libstdc++-devel \
            libstdc++-static \
            sox \
            gcc-c++ \
            libtool \
            autoconf \
            automake \
            m4 \
            netpbm \
            netpbm-progs
