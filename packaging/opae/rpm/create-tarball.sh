#!/bin/sh


SCRIPT_DIR=`dirname $0`
SCRIPT_DIR=`realpath $SCRIPT_DIR`
TOP_DIR=`realpath ${SCRIPT_DIR}/../../..`

# Input is the name of the directory the tarball will
# be unpacked to.
opae_dir=${1:-opae}
trans="s/opae-sdk/${opae_dir}/"

echo $opae_dir

echo ${TOP_DIR}

cd ${TOP_DIR}/..
tar --transform=$trans \
  --exclude=__pycache__ \
  --exclude=_build \
  --exclude=build \
  --exclude=.* \
  --exclude=*~ \
  --exclude=doc/sphinx \
  --exclude=opae-libs/plugins/ase \
  --exclude=external/gtest \
  --exclude=external/opae-legacy/tests \
  --exclude=external/opae-legacy/scripts \
  --exclude=external/opae-legacy/tools/coreidle \
  --exclude=external/opae-legacy/tools/hssi \
  --exclude=external/opae-test \
  --exclude=tools/extra/pyfpgadiag \
  --exclude=tools/extra/pypackager \
  --exclude=packaging/opae/rpm/create-tarball.sh \
  --exclude=packaging/opae/rpm/create   \
  --exclude=packaging/opae/rpm/opae.spec   \
  --exclude=packaging/opae/rpm/clean   \
  -z -c -f ${opae_dir}.tar.gz opae-sdk

