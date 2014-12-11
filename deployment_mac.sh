#!/bin/bash

if [ $# -ne 1 ]
then
  echo "Usage: `basename $0` {build directory}"
  exit 4
fi

QMAKE="/Users/jaeger/Qt5.2.1/5.1.1/clang_64/bin/qmake"
MACDEPLOY="/Users/jaeger/Qt5.2.1/5.2.1/clang_64/bin/macdeployqt"

# make sure that we have the latest version
svn update

# find version
VERSION=$(svnversion -n)
DIRNAME=YUView_$VERSION
BUILD_DIR=$1
SRC_DIR=$(pwd)
PRO_FILE=$SRC_DIR/YUView.pro

# update version.h
echo -n "#define YUVIEW_VERSION \"$VERSION\"" > version.h

# step 0: run qmake+make
$QMAKE $PRO_FILE -r -spec macx-g++ CONFIG+=release CONFIG+=x86_64

make clean -w
rm -rf $BUILD_DIR/YUView.app
make -w

# step 1: make application deployable
$MACDEPLOY $BUILD_DIR/YUView.app

# step 2: copy files to temporary directory
mkdir $DIRNAME
cp -r $BUILD_DIR/YUView.app $DIRNAME/
cp ./docs/YUView\ ToDo.txt $DIRNAME/TODO

# step 3: compress files
ditto -c -k --keepParent $DIRNAME ../$DIRNAME.zip

# step 4: cleanup
rm -rf $DIRNAME/
svn revert version.h
make clean -w
rm -rf $BUILD_DIR/YUView.app