#!/bin/sh
make

#../vbfs_format -e 637 /dev/vdd

./vbfs_fuse /mnt /dev/vdd
ls -al /mnt
mkdir /mnt/test_c
mkdir /mnt/test_a
touch /mnt/file_a
mkdir /mnt/test_c/test_b
touch /mnt/test_c/file_b
echo "#####"
#rm -rf /mnt/*
find /mnt
echo "#####"
dd if=/dev/zero of=/mnt/test.img bs=2M count=8 oflag=direct &
dd if=/dev/zero of=/mnt/test5.img bs=777 count=40960 oflag=direct
dd if=/dev/zero of=/mnt/test2.img bs=3M count=10 oflag=direct &
dd if=/dev/urandom of=/mnt/test.txt bs=512 count=8192 oflag=direct
ls -al /mnt/test.txt
dd if=/dev/urandom of=/mnt/test.txt bs=512 count=4096 oflag=direct
echo "#####"
cat /mnt/noexist.txt
echo "#####"
ls -al /mnt

umount /mnt
#umount -f /mnt
