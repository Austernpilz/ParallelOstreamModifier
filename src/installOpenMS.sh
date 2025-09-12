#!/bin/bash


# you should have downloaded the openms source code from git
# something like git clone https://github.com/OpenMS/OpenMS.git
# as well as the contrib submodules
# git submodule update --init contrib
# git submodule update --init THIRDPARTY (optional)
# and download qt from http://download.qt.io/official_releases/qt

# define your paths
dev=/buffer/ag_bsc/pmsb/mzs

contrib_build=$dev/contrib-build
contrib_src=$OpenMS_src/contrib

qt_src=$dev/qt-everywhere-src-6.5.3
qt_build=$dev/qt6-build

OpenMS_src=$dev/OpenMS
OpenMS_build=$dev/OpenMS-build


# ======== 1. clone openms ========
git clone https://github.com/OpenMS/OpenMS.git $OpenMS_src
cd $OpenMS_src
git submodule update --init contrib

# ======== 2. build openms contrib ========
rm -rf $contrib_build
mkdir -p $contrib_build
cd $contrib_build

# build all contrib libraries using all cores
cmake $contrib_src -dbuild_type=all -dnumber_of_jobs=$(nproc)
cmake --build . --parallel
cmake --install .

cd $dev
wget https://download.qt.io/official_releases/qt/6.9/6.9.2/single/qt-everywhere-src-6.9.2.tar.xz
tar xf qt-everywhere-src-6.9.2.tar.xz
cd qt-everywhere-src-6.9.2

rm -rf qt*
mkdir -p $qt_build
cd $qt_build

cmake $qt_src \
  -dcmake_install_prefix=$qt_build \
  -dcmake_build_type=release \
  -dqt_feature_qttools=off \
  -dqt_feature_qtdoc=off \
  -dqt_feature_qttranslations=off \

cmake --build . --parallel
cmake --install .

# ======== 4. build openms with asan and qt gui ========
rm -rf $OpenMS_build $OpenMS_build
mkdir -p $OpenMS_build
cd $OpenMS_build

cmake $OpenMS_src \
  -dcmake_build_type=debug \
  -denable_update_check=off \
  -dopenms_contrib_libs="$contrib_build" \
  -dcmake_prefix_path="$HOME/.local" \
  -dqt6_dir="$qt_build/lib/cmake/Qt6" \
  -dcmake_install_prefix="$OpenMS_build" \
  -dwith_gui=on \
  -dboost_use_static=on \
  -dboost_no_boost_cmake=on \
  -dboost_use_debug_libs=on \
  -dboost_architecture="-x64" \
  -dmy_cxx_flags="-Og -ggdb -g3 -fno-omit-frame-pointer -fsanitize=address" \
  -dopenms_use_address_sanitizer=on

make -j$(nproc) verbose=1
make install
