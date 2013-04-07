#!/bin/bash
echo "Cleaning Kernel Tree"
echo "If you haven done make menuconfig"
echo "please do so before running this"
make clean
# Edit CROSS_COMPILE to mirror local path. Edit "version" to any desired name or number but it cannot have spaces. 
pwd=`readlink -f .`
export ARCH=arm
export version=Jaded_Jackalope

# Determines the number of available logical processors and sets the work thread accordingly
export JOBS="(expr 4 + $(grep processor /proc/cpuinfo | wc -l))"

date=$(date +%Y%m%d-%H:%M:%S)

# Check for a log directory in ~/ and create if its not there
[ -d ~/logs ] || mkdir -p ~/logs

# Build kernel
make ARCH=arm | tee ~/logs/$version.txt

echo "Popped kernel available in arch/arm/boot"

# Build Modules and output build log
time make -j8 2>&1
echo "Build Log is available in ~/logs"
echo "Done"

geany ~/logs/$version.txt || exit 1
