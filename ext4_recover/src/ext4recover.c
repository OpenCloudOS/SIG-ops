/*
 * History:
 * 2020-03-01	- Creation by zorrozou
 * 2020-03-08	- beta 0.1
 * 2020-08-05   - beta 0.2 split from e2fsprogs code by curuwang
 */

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/types.h>
#include <ext2fs/ext2fs.h>
#include <sys/stat.h>

#define RECOVER_DIR "./RECOVER"
#define VERSION "0.2b"

static const char *program_name = "ext4recover";
static char *device_name = NULL;
static int flag = 0;
static __u32 icount;
static struct ext3_extent_header *eh;
static struct ext3_extent_idx *ei;
static struct ext3_extent *ee;
static ssize_t blocksize;
static int recover_fd, device_fd;

struct extent_path {
    char *buf;
    int entries;
    int max_entries;
    int left;
    int visit_num;
    int flags;
    blk64_t end_blk;
    void *curr;
};

struct ext2_extent_handle {
    errcode_t magic;
    ext2_filsys fs;
    ext2_ino_t ino;
    struct ext2_inode *inode;
    struct ext2_inode inodebuf;
    int type;
    int level;
    int max_depth;
    int max_paths;
    struct extent_path *path;
};

static int is_inode_extent_clear(struct ext2_inode *inode) {
    errcode_t retval;

    eh = (struct ext3_extent_header *) inode->i_block;
    ei = (struct ext3_extent_idx *) eh + 1;
    ee = (struct ext3_extent *) eh + 2;
    blk64_t blk;
    blk = ext2fs_le32_to_cpu(ei->ei_leaf) +
          ((__u64) ext2fs_le16_to_cpu(ei->ei_leaf_hi) << 32);
    if (ei->ei_leaf > 1 && LINUX_S_ISREG(inode->i_mode) && inode->i_links_count == 0) {
        fprintf(stdout, "inode: %u extent_block: %llu\n", icount, blk);
        // not clear
        return 0;
    }

    return 1;
}

static int recover_block_to_file(int devfd, int inofd, __le32 block, __le16 len, __u64 start) {
    off_t offset_dev, offset_ino, offset;
    ssize_t size, ret;
    char buf[blocksize];
    int i;

    offset_dev = lseek(devfd, start * blocksize, SEEK_SET);
    if (offset_dev < 0) {
        perror("lseek(devfd)");
        return 0;
    }

    offset_ino = lseek(inofd, block * blocksize, SEEK_SET);
    if (offset_ino < 0) {
        perror("lseek(inofd)");
        return 0;
    }

    for (i = 0; i < len; i++) {
        offset = 0;
        size = 0;
        while (offset < blocksize) {
            size = read(devfd, buf + offset, blocksize - offset);
            if (size < 0) {
                lseek(devfd, offset_dev + blocksize, SEEK_SET);
                perror("read(dev_fd)");
                continue;
            }
            offset += size;
        }
        //printf("before write offset: %llu\n", offset);
        //printf("before write size: %llu\n", size);
        while (offset != 0) {
            ret = write(inofd, buf + (blocksize - offset), offset);
            if (ret < 0) {
                lseek(inofd, offset_ino + blocksize, SEEK_SET);
                perror("write(inofd)");
                continue;
            }
            offset -= ret;
            //printf("ret: %llu\n", ret);
        }
        //printf("after write offset: %llu\n", offset);
        //printf("after write size: %llu\n", size);
    }

    offset_dev = lseek(devfd, 0, SEEK_SET);
    if (offset_dev < 0) {
        perror("lseek(devfd)");
        return 0;
    }

    offset_ino = lseek(inofd, 0, SEEK_SET);
    if (offset_ino < 0) {
        perror("lseek(inofd)");
        return 0;
    }
    return 1;
}

static int dump_dir_extent(struct ext3_extent_header *eh) {
    struct ext3_extent *ee;
    int i;
    __le32 ee_block;
    __le16 ee_len;
    __u64 ee_start;
    char *buf;
    __u32 headbuflen = 4;
    int retval;

/*
	retval = ext2fs_get_mem(headbuflen, &buf);
	if (retval)
		return;
*/
    ee = EXT_FIRST_EXTENT(eh);
//printf("eh->eh_entries: %d\n", eh->eh_entries);
//printf("eh->eh_max: %d\n", eh->eh_max);
    if (ext2fs_le16_to_cpu(eh->eh_entries) > 340) {
        return 1;
    } else if (ext2fs_le16_to_cpu(eh->eh_magic) != EXT3_EXT_MAGIC) {
        return 1;
    } else if (ext2fs_le16_to_cpu(eh->eh_max) != 340) {
        return 1;
    }
    for (i = 1; i < eh->eh_entries + 1; i++) {
        ee_block = ext2fs_le32_to_cpu(ee->ee_block);
        ee_len = ext2fs_le32_to_cpu(ee->ee_len);
        ee_start = (((__u64) ext2fs_le16_to_cpu(ee->ee_start_hi) << 32) +
                    (__u64) ext2fs_le32_to_cpu(ee->ee_start));
        printf("%u %u %u %llu\n", icount, ee_block, ee_len, ee_start);
        fflush(stdout);

        retval = recover_block_to_file(device_fd, recover_fd, ee_block, ee_len, ee_start);
        if (!retval) {
            fprintf(stderr, "recover_block_to_file()\n");
            return 0;
        }

        ee++;
    }
    return 1;
}

static int extent_tree_travel(ext2_extent_handle_t handle, struct ext3_extent_header *eh) {
    struct ext3_extent_header *next;
    struct ext3_extent_idx *ei;
    int i, retval;
    char *buf;
    blk64_t blk;

    //printf("Enter extent_tree_travel
    if (eh->eh_depth == 0) {
        //printf("dump_dir_extent\n");
        retval = dump_dir_extent(eh);
        if (!retval) {
            fprintf(stderr, "dump_dir_extent()\n");
            return retval;
        }
    } else if (eh->eh_depth <= 4) {
        flag = 1;
        //printf("eh->eh_depth < 4\n");
        for (i = 1; i < eh->eh_entries + 1; i++) {
            retval = ext2fs_get_mem(blocksize, &buf);
            if (retval)
                return;

            memset(buf, 0, blocksize);
            ei = EXT_FIRST_INDEX(eh) + i - 1;
            blk = ext2fs_le32_to_cpu(ei->ei_leaf) +
                  ((__u64) ext2fs_le16_to_cpu(ei->ei_leaf_hi) << 32);
            retval = io_channel_read_blk64(handle->fs->io,
                                           blk, 1, buf);
            if (retval)
                return retval;
            next = (struct ext3_extent_header *) buf;
            //printf("Recursive\n");
            retval = extent_tree_travel(handle, next);
            if (!retval) {
                fprintf(stderr, "extent_tree_travel()\n");
            }
            ext2fs_free_mem(&buf);
            //printf("Recursive end\n");
        }
    } else {
        /* xxxxxxxxxxxxxxx */
        return 1;
    }
    return 1;
}

static int prase_ino_extent(ext2_extent_handle_t handle) {
    int i, retval;
    struct ext3_extent_idx *ix;
    struct ext3_extent_header *next;
    char *buf;
    blk64_t blk;
    struct ext3_extent_header *eh;

    eh = (struct ext3_extent_header *) handle->inode->i_block;

//	printf("aaaaaaaaaaaaaa\n");
    retval = ext2fs_get_mem(blocksize, &buf);
    if (retval)
        return 0;
//		printf("i:%d\n", i);
//		printf("ix->ei_leaf: %d\n", ix->ei_leaf);
//		printf("next->eh_max: %d\n", next->eh_max);
//		printf("next->eh_entries: %d\n", next->eh_entries);
    memset(buf, 0, blocksize);
    for (i = 1; i <= 4; i++) {
        ix = EXT_FIRST_INDEX(eh) + i - 1;


        blk = ext2fs_le32_to_cpu(ix->ei_leaf) +
              ((__u64) ext2fs_le16_to_cpu(ix->ei_leaf_hi) << 32);
        retval = io_channel_read_blk64(handle->fs->io,
                                       blk, 1, buf);
        if (retval)
            return retval;

        next = (struct ext3_extent_header *) buf;
        //printf("bbbbbbbbbbbbbb\n");
        retval = extent_tree_travel(handle, next);
        if (!retval) {
            fprintf(stderr, "extent_tree_travel()");
            return retval;
        }
        //printf("cccccccccccccccccc\n");
        /* for last extent */
        if (next->eh_entries < 340) {
//		printf("dddddddddddddddddddd\n");
            break;
        }
    };
    ext2fs_free_mem(&buf);
    return 1;
}

static int is_on_device(const char *path, const char *dev) {
    struct stat stat1, stat2;
    int ret;
    ret = stat(path, &stat1);
    if (ret < 0) {
        perror("stat:");
        return -1;
    }
    ret = stat(dev, &stat2);
    if (ret < 0) {
        perror("stat:");
        return -1;
    }
    if (((stat2.st_mode & S_IFMT) == S_IFBLK) && (stat1.st_dev == stat2.st_rdev)) {
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    errcode_t retval;
    blk64_t use_superblock = 0;
    ext2_filsys fs;
    int use_blocksize = 0;
    int flags;
    __u32 imax;
    struct ext2_inode inode;
    ext2_extent_handle_t handle;
    char filename[BUFSIZ];

    if (argc != 2) {
        fprintf(stderr, "Usage: %s /dev/xxx\n", argv[0]);
        fprintf(stderr, "Recover deleted files using remaining extent info\n");
        fprintf(stderr, "version: %s\n\n", VERSION);
        fprintf(stderr, "eg:\n\t%s /dev/vdb1\n", argv[0]);
        exit(1);
    }

    device_name = argv[1];
    flags = EXT2_FLAG_JOURNAL_DEV_OK | EXT2_FLAG_SOFTSUPP_FEATURES |
            EXT2_FLAG_64BITS;
    retval = ext2fs_open(device_name, flags, use_superblock,
                         use_blocksize, unix_io_manager, &fs);

    if (retval) {
        com_err(program_name, retval, "while trying to open %s",
                device_name);
        printf("%s", "Couldn't find valid filesystem superblock.\n");
        exit(retval);;
    }
    fs->default_bitmap_type = EXT2FS_BMAP64_RBTREE;

    blocksize = fs->blocksize;

    device_fd = open(argv[1], O_RDONLY);
    if (device_fd < 0) {
        perror("open(device)");
        exit(1);
    }

    retval = mkdir(RECOVER_DIR, 0750);
    if (retval < 0 && errno != EEXIST) {
        perror("mkdir()");
        exit(1);
    }
    if (is_on_device(RECOVER_DIR, device_name) == 1) {
        fprintf(stderr, "DANGER: recover dir '%s' is on target device '%s', aborted!\n",
                RECOVER_DIR, device_name);
        exit(1);
    }
    imax = fs->super->s_inodes_count;
    for (icount = 3; icount < imax + 1; icount++) {
        flag = 0;

        //	printf("%u\n", icount);
        retval = ext2fs_read_inode(fs, icount, &inode);
        if (retval) {
            com_err(program_name, retval, "%s",
                    "while reading journal inode");
            continue;
        }
        if (is_inode_extent_clear(&inode)) {
            continue;
        }

        snprintf(filename, BUFSIZ, "%s/%d_file", RECOVER_DIR, icount);

        recover_fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC | O_LARGEFILE, 0640);
        if (recover_fd < 0) {
            perror("open(inode)");
            continue;
        }

        retval = ext2fs_extent_open(fs, icount, &handle);
        if (retval)
            return;

        retval = prase_ino_extent(handle);
        if (!retval) {
            fprintf(stderr, "Recover error!\n");
            close(recover_fd);
            close(device_fd);
            exit(1);
        }
        if (flag) {
            fflush(stdout);
        }

        close(recover_fd);
    }

    close(device_fd);

    fprintf(stderr, "Recover success!\n");

    exit(0);
}

