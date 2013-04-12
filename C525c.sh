#!/bin/bash
echo "Cleaning Kernel Tree"
echo "If you haven done make menuconfig"
echo "please do so before running this"
make clean
rm output
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

# Copy Modules to output Folder
mkdir output

cp arch/arm/mach-msm/reset_modem.ko output/
cp arch/arm/mach-msm/dma_test.ko output/
cp arch/arm/mach-msm/msm-buspm-dev.ko output/
cp crypto/ansi_cprng.ko output/
cp drivers/video/backlight/lcd.ko output/
cp drivers/misc/eeprom/eeprom_93cx6.ko output/
cp drivers/scsi/scsi_wait_scan.ko output/
cp drivers/spi/spidev.ko kernel/output/
cp drivers/net/wireless/bcmdhd_4334/bcmdhd.ko output/
cp drivers/net/ks8851.ko output/
cp drivers/input/evbug.ko output/
cp drivers/media/video/gspca/gspca_main.ko output/
cp drivers/bluetooth/bluetooth-power.ko output/
cp drivers/mmc/card/mmc_test.ko output/
cp drivers/crypto/msm/qcedev.ko output/
cp drivers/crypto/msm/qce40.ko output/
cp drivers/crypto/msm/qcrypto.ko output/

echo "Done"
geany ~/logs/$version.txt || exit 1
