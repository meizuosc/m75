# m75
M75 repo is Linux kernel source code for Meizu MX4 Ubuntu edition smartphones. With this repo, you can customize the source code and compile a Linux kernel image yourself. Enjoy it!

Extra packages needed to compile in ubuntu
------------------------------------
sudo apt-get install gcc-arm-linux-androideabi lzop abootimg

Build the kernel
----------------
./makeMtk -t m75 n k

Get latest initrd
----------------
git submodule init  
git submodule update

Build a boot.img 
----------------
cd testboot && ./mkbootimg.sh

Export the tree
---------------
./export.sh
