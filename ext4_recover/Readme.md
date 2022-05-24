# 项目介绍
ext4文件系统误删除文件恢复工具

## 支持的平台

Linux ext4

# 快速上手     Getting Started

## 环境要求
Linux

## 前提条件


## 操作步骤
### 使用方法
```
ext4recover /dev/sdx
# 假设/dev/sdx是要恢复数据的设备，执行上述命令后，会将能恢复的已删除文件恢复到RECOVER子目录下。
```
### 编译方法
```
yum install -y e2fsprogs-devel libcom_err-devel
cd src
make
```

## 常见问题
#### 原理:
ext4在删除包含多级extent的文件时，会调用ext4_ext_rm_idx 删除下一级， 上一级本身只是eh_entries减一，
但是block位置没修改，同时inode外部的extent块信息清空后没有落盘。所以包含多级extent的文件删除后可以利用这些信息恢复。
只包含一级extent时，只调用ext4_ext_rm_leaf,会修改对应index的eh_entries, 并且每一个entry的block位置被置0， 长度置0,
所以只包含一级extent的文件无法恢复.

如果文件不存在碎片,单文件大于496M就会使用多级extent, 能恢复。
当存在碎片的情况，文件就算很小也可能占用4个以上extent, 因此这些高度碎片化的小文件也能恢复

背景信息：ext4_ext_rm_idx删除的外部extent数据没落盘其实算是一个bug, 只不过当前内核社区还没修复, 
brookxu同学提了补丁：https://lkml.org/lkml/2020/3/5/1248

本工具原始版本是zorrozou所写，curuwang从e2fsprogs代码中剥离并补充原理说明。

为什么说是bug呢? 因为实际上内核代码里面已经将叶子extent清空了(
rm后，直接dd读底层的extent,发现已清0, 但是dd使用iflag=direct绕过pagecache从块设备发现信息有残留,
证明只是把页面写了没有标记未脏，所以没落盘)

文件被删除后, inode里面的信息发生了如下变化:
1. 文件大小,文件链接数变为0
2. 文件dtime,ctime,atime,mtime被设置为删除时间
3. 上述的extent信息变化
4. 其他...
