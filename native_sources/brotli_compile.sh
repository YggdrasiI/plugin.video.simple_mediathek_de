#!/bin/bash
# Fetch and compile required libraries
#

MACH=$1  # Presetting by makefile
test "$MACH" = "" && MACH=$(uname -m)

J=$(cat /proc/cpuinfo  | grep "processor" | wc -l)

if [ -n "$2" ] ; then
  COMPILER_FLAGS='-DCMAKE_C_FLAGS:STRING='$2' '
	echo "Flags: $COMPILER_FLAGS"
fi

# Path relative to brotli/release
#INSTALL_FOLDER="../../root"
INSTALL_FOLDER="../../../root/${MACH}"

if [ -f "brotli/release_${MACH}/${INSTALL_FOLDER}/lib/libbrotlidec.so" -a "${FORCE}" != "1" ] ; then
  echo "Brotli library already exists and rebuild skiped. Call Makefile with " \
    "'FORCE=1 make [...] to force new build."
else

  test -d "./brotli"  || git clone --depth 1 "https://github.com/google/brotli.git"
  cd brotli/

  test -d "./release_${MACH}" || mkdir "release_${MACH}"
  cd "release_${MACH}"

  cmake -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX:PATH="$INSTALL_FOLDER" \
    -DBUILD_TESTING:BOOL=OFF \
    -DCMAKE_INSTALL_LIBDIR:PATH="lib" \
    "${COMPILER_FLAGS}" \
    ..


  if [ "${FORCE}" = "1" ] ; then
    make clean
  fi
  make -j$J && \

  echo "Brotli compiled. Run 'make install' to copy"
  echo "libs and headers into $INSTALL_FOLDER."

  # make install
fi
