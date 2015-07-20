#!/bin/bash
set -e
[ -n $ROOT ] && ROOT=".."
LOCAL_TMP=`mktemp -d`
MKIMAGE=$ROOT/mediatek/build/tools/mkimage
CORE_NAME=vivid_overlay

pushd $ROOT
git submodule update --init
[ -d ubuntu/ubuntu_prebuilt_initrd_debs/$CORE_NAME ] || {
    echo "warning: $CORE_NAME not exist. Will delete ubuntu/ubuntu_prebuilt_initrd_debs. And checkout out it again."
    rm -rf ubuntu/ubuntu_prebuilt_initrd_debs
    git submodule add https://code-review.phablet.ubuntu.com/p/ubuntu/initrd/ubuntu_prebuilt_initrd_debs ubuntu/ubuntu_prebuilt_initrd_debs
}
popd
dpkg-deb -x $ROOT/ubuntu/ubuntu_prebuilt_initrd_debs/$CORE_NAME/armhf/ubuntu-touch-generic-initrd*.deb $LOCAL_TMP
$MKIMAGE $LOCAL_TMP/usr/lib/ubuntu-touch-generic-initrd/initrd.img-touch ROOTFS > initrd.img
rm -rf $LOCAL_TMP

