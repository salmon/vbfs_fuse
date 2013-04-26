#!/bin/sh
rm -rf /mnt/*
./vbfs_fuse /mnt /dev/vdc
sleep 1
ls /mnt
ls /mnt -ali
#touch /mnt/test_file
#mkdir /mnt/test_dir
sleep 1
umount -f /mnt
