#!/bin/sh
make

#../vbfs_format -e 192 /dev/sda

#./vbfs_fuse -o /mnt /dev/sda
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
#rm -rf /mnt/*
#find /mnt
ls -al /mnt
echo "#####"
dd if=/dev/zero of=/mnt/test.img bs=2M count=8 oflag=direct &
dd if=/dev/zero of=/mnt/test1.img bs=777 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test2.img bs=1028 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test3.img bs=666 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test4.img bs=896 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test5.img bs=60 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test6.img bs=2058 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test7.img bs=683 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test8.img bs=521 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test9.img bs=398 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test10.img bs=657 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test11.img bs=631 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test12.img bs=487 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test13.img bs=698 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test14.img bs=790 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test15.img bs=888 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test16.img bs=999 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test17.img bs=1407 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test18.img bs=1578 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test19.img bs=2056 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test20.img bs=2577 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test21.img bs=146 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test22.img bs=502 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test23.img bs=1955 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test24.img bs=882 count=409600000000 oflag=direct &
dd if=/dev/zero of=/mnt/test25.img bs=1067 count=409600000000 oflag=direct
#dd if=/dev/zero of=/mnt/test2.img bs=3M count=10 oflag=direct &
#dd if=/dev/urandom of=/mnt/test.txt bs=512 count=8192 oflag=direct
#ls -al /mnt/test.txt
#dd if=/dev/urandom of=/mnt/test.txt bs=512 count=4096 oflag=direct
echo "#####"
#cat /mnt/noexist.txt
#echo "#####"
ls -al /mnt
sleep 1

umount /mnt
#umount -f /mnt
