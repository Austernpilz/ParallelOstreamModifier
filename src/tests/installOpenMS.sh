#!/bin/bash


# you should have downloaded the openms source code from git
# something like git clone https://github.com/OpenMS/OpenMS.git
# as well as the contrib submodules
# git submodule update --init contrib
# git submodule update --init THIRDPARTY (optional)
# and download qt from http://download.qt.io/official_releases/qt

# define your paths
dev=/buffer/ag_bsc/pmsb/mzs

contrib_build=/buffer/ag_bsc/pmsb/mzs/contrib-build
contrib_src=/buffer/ag_bsc/pmsb/mzs/OpenMS/contrib

qt_src=/buffer/ag_bsc/pmsb/mzs/qt-everywhere-src-6.5.3
qt_build=/buffer/ag_bsc/pmsb/mzs/qt6-build

openms_src=/buffer/ag_bsc/pmsb/mzs/OpenMS
openms_debug=/buffer/ag_bsc/pmsb/mzs/OpenMS-build/debug
openms_release=/buffer/ag_bsc/pmsb/mzs/OpenMS-build/release

# clone OpenMS
git clone https://github.com/OpenMS/OpenMS.git $openms_src
cd $openms_src
git submodule update --init contrib

# builupdate and build contrib
# rm -rf $contrib_build
mkdir -p $contrib_build
cd $contrib_build

cmake $contrib_src -DBUILD_TYPE=ALL -DNUMBER_OF_JOBS=$(nproc)
cmake $contrib_src -DBUILD_TYPE=EIGEN -DNUMBER_OF_JOBS=$(nproc)

# get qt and install

# rm -rf qt*
cd $dev
wget https://download.qt.io/official_releases/qt/6.5/6.5.3/single/qt-everywhere-src-6.5.3.tar.xz
tar xf qt-everywhere-src-6.5.3.tar.xz

mkdir -p $qt_build
cd $qt_src
#u can only have a messy instalation becaue qt doesn't care

./configure --prefix=. \
            -skip qttools \
            -skip qtdoc \
            -skip qttranslations

cmake --build . --parallel --VERBOSE=1 
cmake --install .

# finally build OpenMS
# rm -rf $openms_build
mkdir -p $openms_build
cd $openms_debug

cmake $openms_src \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_UPDATE_CHECK=OFF \
  -DOPENMS_CONTRIB_LIBS="$contrib_build" \
  -DCMAKE_PREFIX_PATH="$HOME/.local" \
  -DQt6_DIR="$qt_src/lib/cmake/Qt6" \
  -DCMAKE_INSTALL_PREFIX="$openms_debug" \
  -DBOOST_USE_STATIC=ON \
  -DBoost_NO_BOOST_CMAKE=ON \
  -DBoost_USE_DEBUG_LIBS=ON \
  -DBoost_ARCHITECTURE="-x64" \
  -DMY_CXX_FLAGS="-Og -ggdb -g3 -fno-omit-frame-pointer -fsanitize=address" \
  -DOPENMS_USE_ADDRESS_SANITIZER=ON

cmake $openms_src \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_UPDATE_CHECK=OFF \
  -DOPENMS_CONTRIB_LIBS="$contrib_build" \
  -DCMAKE_PREFIX_PATH="$HOME/.local" \
  -DQt6_DIR="$qt_build/lib/cmake/Qt6" \
  -DCMAKE_INSTALL_PREFIX="$openms_release" \
  -DBOOST_USE_STATIC=ON \
  -DBoost_NO_BOOST_CMAKE=ON \
  -DBoost_ARCHITECTURE="-x64"

DENABLE_UPDATE_CHECK=Off \
-DCMAKE_BUILD_TYPE=Debug \
-DOPENMS_CONTRIB_LIBS=/buffer/ag_bsc/pmsb_openms_contrib/contrib_build \
-DBOOST_USE_STATIC=ON \
-DQt6_DIR=/buffer/ag_bsc/pmsb_openms_contrib/qt-everywhere-src-6.7.3/lib/cmake/Qt6/ 
cmake --build . --parallel --VERBOSE=1 
cmake --install .