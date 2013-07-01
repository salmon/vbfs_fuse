#!/bin/sh
make

#../vbfs_format -e 673 /dev/vdd

./vbfs_fuse /mnt /dev/vdd
ls -al /mnt
#mkdir /mnt/test_c
#mkdir /mnt/test_a
#touch /mnt/file_a
#mkdir /mnt/test_c/test_a
#mkdir /mnt/test_c/test_b
#touch /mnt/test_c/file_b
echo "#####"
dd if=/dev/zero of=/mnt/test5.img bs=2M count=4 oflag=direct
echo "#####"
rm -rf /mnt/*
find /mnt
ls -al /mnt
echo "#####"
#dd if=/dev/zero of=/mnt/test.img bs=2M count=8 oflag=direct &
dd if=/dev/zero of=/mnt/test1.img bs=777 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test2.img bs=777 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test3.img bs=777 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test4.img bs=777 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test5.img bs=777 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test6.img bs=777 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test7.img bs=777 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test8.img bs=777 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test9.img bs=777 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test10.img bs=777 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test11.img bs=777 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test12.img bs=777 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test13.img bs=777 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test14.img bs=777 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test15.img bs=777 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test16.img bs=777 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test17.img bs=777 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test18.img bs=777 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test19.img bs=777 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test20.img bs=777 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test21.img bs=777 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test22.img bs=777 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test23.img bs=777 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test24.img bs=777 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test25.img bs=777 count=409600000000 oflag=direct
#dd if=/dev/zero of=/mnt/test2.img bs=3M count=10 oflag=direct &
#dd if=/dev/urandom of=/mnt/test.txt bs=512 count=8192 oflag=direct
#ls -al /mnt/test.txt
#dd if=/dev/urandom of=/mnt/test.txt bs=512 count=4096 oflag=direct
echo "#####"
#cat /mnt/noexist.txt
#echo "#####"
ls -al /mnt

umount /mnt
#umount -f /mnt
