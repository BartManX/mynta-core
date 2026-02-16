#!/bin/bash
cd /mnt/c/Users/Drock/Documents/.Mynta-Workspace/mynta-core
DEPENDS=$PWD/depends/x86_64-w64-mingw32
export CPPFLAGS="-I$DEPENDS/include"
export LDFLAGS="-L$DEPENDS/lib"
export PKG_CONFIG_PATH="$DEPENDS/lib/pkgconfig"
export PKG_CONFIG="pkg-config --static"
./configure \
    --host=x86_64-w64-mingw32 \
    --prefix=/ \
    --disable-tests \
    --disable-bench \
    --with-boost=$DEPENDS \
    --with-qt-plugindir=$DEPENDS/plugins \
    --with-qt-translationdir=$DEPENDS/translations \
    --with-qt-bindir=$DEPENDS/native/bin
