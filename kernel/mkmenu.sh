#!/bin/bash

KBUILD_OUTPUT=../out/target/product/m75/obj/KERNEL_OBJ
PROJECT=m75
CONFIG_FILE_SRC=$KBUILD_OUTPUT/.config
CONFIG_FILE_DST=../mediatek/config/m75/autoconfig/kconfig/project

export PROJECT
export KBUILD_OUTPUT
if [ -z $1 ];then
	make menuconfig
else
if [ $1 = 'n' ];then
	make nconfig
else
make  menuconfig 
fi
fi

if [ $? != 0 ];then
	echo "make menuconfig fail!"
	exit $?;
fi

read -p "Copy .config to m75 project directory?[Y/N]" op

while true; do
case $op in 
Y|y) 
	echo "copy $CONFIG_FILE_SRC to $CONFIG_FILE_DST"
	cp $CONFIG_FILE_SRC $CONFIG_FILE_DST 
	break
	;;

N|n)
	break
	;;
*)
	read -p "Copy .config to m75 project directory?[Y/N]" op
	;;
esac
done
