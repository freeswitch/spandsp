#!/bin/sh
#
# Install the things which need adding to a fresh Debian 11 install to make it ready to build
# spandsp and its test suite
#
apt-get install libfftw3-dev \
                libtiff-dev \
                libtiff-tools \
                libpcap-dev \
                libxml2-dev \
                libsndfile-dev \
                libuv1-dev \
                libfltk1.3-dev \
                sox \
                libtool \
                netpbm

#               doxygen \
#               xsltproc\
#               docbook \
#               docbook-xsl
