#!/bin/bash
## Usage:

set -e
set -x

BUILD_PATH="/tmp/ptabuild"
PACKAGE_NAME="platopia"
SRC_PATH=`pwd`
SRC_DEB=$SRC_PATH/contrib/debian
SRC_DOC=$SRC_PATH/doc

umask 022

if [ ! -d $BUILD_PATH ]; then
    mkdir $BUILD_PATH
fi

PACKAGE_VERSION=$($SRC_PATH/src/platopiad --version | grep version | cut -d' ' -f5 | tr -d v)
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
    PACKAGE_VERSION=$DESC
else [ -n "$SUFFIX" ];
    PACKAGE_VERSION=$SUFFIX
fi

DEBVERSION=$(echo ${PACKAGE_VERSION:1} | sed 's/_beta/~beta/' | sed 's/_rc/~rc/' | sed 's/-/+/')
BUILD_DIR="$BUILD_PATH/$PACKAGE_NAME-$PACKAGE_VERSION-amd64"

if [ -d $BUILD_DIR ]; then
    rm -R $BUILD_DIR
fi

DEB_BIN=$BUILD_DIR/usr/bin
DEB_CMP=$BUILD_DIR/usr/share/bash-completion/completions
DEB_DOC=$BUILD_DIR/usr/share/doc/$PACKAGE_NAME
mkdir -p $BUILD_DIR/DEBIAN $DEB_CMP $DEB_BIN $DEB_DOC $DEB_MAN
chmod 0755 -R $BUILD_DIR/*

# Copy binaries
cp $SRC_PATH/src/platopiad $DEB_BIN
cp $SRC_PATH/src/platopia-cli $DEB_BIN
cp $SRC_PATH/src/platopia-seeder $DEB_BIN

# Copy bash completion files
cp $SRC_PATH/contrib/bitcoind.bash-completion $DEB_CMP/platopiad
#cp $SRC_PATH/contrib/bitcoin-cli.bash-completion $DEB_CMP/platopia-cli

cd $SRC_PATH/contrib

# Create the control file
dpkg-shlibdeps $DEB_BIN/platopiad $DEB_BIN/platopia-cli
dpkg-gencontrol -P$BUILD_DIR -v$DEBVERSION

# Create the Debian package
fakeroot dpkg-deb --build $BUILD_DIR
cp $BUILD_PATH/$PACKAGE_NAME-$PACKAGE_VERSION-amd64.deb $SRC_PATH
# Analyze with Lintian, reporting bugs and policy violations
#lintian -i $SRC_PATH/$PACKAGE_NAME-$PACKAGE_VERSION-amd64.deb
exit 0
