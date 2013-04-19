#!/bin/bash

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

exit
