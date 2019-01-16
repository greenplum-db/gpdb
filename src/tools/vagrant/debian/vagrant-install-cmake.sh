set -e

CMAKE_VERSION=3.13.2

PREV_DIR=$PWD
cd /tmp
wget "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}.tar.gz"
tar zxf "cmake-${CMAKE_VERSION}.tar.gz"
cd "cmake-$CMAKE_VERSION"
./bootstrap
make
sudo make install
cd "$PREV_DIR"
