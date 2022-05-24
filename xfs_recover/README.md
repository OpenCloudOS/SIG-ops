# Xfs文件系统误删除文件恢复工具使用

这是一个xfs误删除的恢复工具，使用方法：

编译:
```
make
```

如果make失败的话，需安装以下rpm包再make：
centos:
```
yum install libtool
yum install libuuid libuuid-devel  
yum install  libblkid-devel 
```
ubuntu:
```
apt get glibtoolize
apt install uuid-dev
apt install libblkid-dev
```

make成功后，在 recover目录下会编译生成 xfsrecover 可执行文件。

之后 cd 到你想恢复数据的目标目录，比如此处目标目录为 /data1 , xfs_recover目录为 /data1/xfs_recover ，执行：

cd /data1

./xfs_recover/db/xfsrecover /dev/sdb1      # /dev/sdb1 为你想恢复数据的文件系统。

执行完毕后会在 /data1 下产生 RECOVER 目录，其中的文件就是恢复出来的数据。

