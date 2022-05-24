cmake_minimum_required(VERSION 2.8)
project(ext4_recover)

add_executable(ext4recover src/ext4recover.c)

find_library(EXT2FS ext2fs)
find_library(COM_ERR com_err)
target_link_libraries(ext4recover ${EXT2FS})
target_link_libraries(ext4recover ${COM_ERR})