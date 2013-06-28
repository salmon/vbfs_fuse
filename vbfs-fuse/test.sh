#!/bin/sh
make

./vbfs_fuse /mnt /dev/vdd
ls -al /mnt
#mkdir /mnt/test_c
#mkdir /mnt/test_a
#touch /mnt/file_a
#mkdir /mnt/test_c/test_b
#touch /mnt/test_c/file_b
echo "#####"
dd if=/dev/zero of=/mnt/test.img bs=2M count=2
echo "11111111" > /mnt/test.txt
echo "22222222" >> /mnt/test.txt
echo "#####"
cat /mnt/test.txt
echo "#####"
ls -al /mnt

umount /mnt
#umount -f /mnt
