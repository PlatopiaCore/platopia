#!/bin/sh -x
# Copyright (c) 2013-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

set -e
srcdir="$(dirname $0)"
cd "$srcdir"
if [ -z ${LIBTOOLIZE} ] && GLIBTOOLIZE="`which glibtoolize 2>/dev/null`"; then
  LIBTOOLIZE="${GLIBTOOLIZE}"
  export LIBTOOLIZE
fi
which autoreconf >/dev/null || \
  (echo "configuration failed, please install autoconf first" && exit 1)
autoreconf --install --force --warnings=all

platfrom=$(uname -a|cut -d' ' -f1)
symbol=""
if [ $platfrom == "Darwin" ];then
    symbol="back"
fi

DESC=""
SUFFIX=""
if [ -e "$(which git 2>/dev/null)" -a "$(git rev-parse --is-inside-work-tree 2>/dev/null)" = "true" ]; then
    # clean 'dirty' status of touched files that haven't been modified
    git diff >/dev/null 2>/dev/null

    # if latest commit is tagged and not dirty, then override using the tag name
    RAWDESC=$(git rev-list --tags --max-count=1 2>/dev/null)
    if [ "$(git rev-parse HEAD)" = "$(git rev-list -1 $RAWDESC 2>/dev/null)" ]; then
        git diff-index --quiet HEAD -- && DESC=$(git describe --tags $RAWDESC 2>/dev/null)
    fi

    # otherwise generate suffix from git, i.e. string like "59887e8-dirty"
    SUFFIX=$(git describe --tags $RAWDESC 2>/dev/null)
    git diff-index --quiet HEAD -- || SUFFIX="$SUFFIX-dirty"
fi


if [ -n "$DESC" ]; then
    sed -i $symbol "s/PACKAGE_VERSION=.*$/PACKAGE_VERSION='$DESC'/" ./configure
else [ -n "$SUFFIX" ];
    sed -i $symbol "s/PACKAGE_VERSION=.*$/PACKAGE_VERSION='$SUFFIX'/" ./configure
fi
