#!/bin/bash
set -e
# get the ubuntu ramdisk, initrd.img
ROOT=".."
PRODUCT_OUT="$ROOT/out/target/product/m75"
./get_latest_initrd.sh
# generate boot.img
abootimg --create boot.img -k $PRODUCT_OUT/kernel_m75.bin -r initrd.img -f bootimg.cfg
