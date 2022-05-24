/*
 * Copyright 2020 Tencent Inc.  All rights reserved.
 * Author: curuwang@tencent.com
*/
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <byteswap.h>

#define BLOCK_SIZE 16*1024*1024
#define MAX_VALID_BLOCKS 131072
#define RECOVER_DIR "./RECOVER"

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <device_path> <chunk_size>\n", argv[0]);
        exit(1);
    }

	setlinebuf(stdout);

    char *f = argv[1];
	size_t block_size = atoi(argv[2]);
    printf("checking %s with chunk: %d bytes\n", f, block_size);

	char *zero_block = malloc(block_size);
	char *buf = malloc(block_size);
	memset(zero_block, '\0', block_size);

    int fd = open(f, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    size_t nread, blk_num=0;
	ssize_t blk_start= -1;
    while (nread = read(fd, buf, block_size)) {
		if(memcmp(buf, zero_block, block_size) != 0){
			blk_start = blk_start > -1 ? blk_start : blk_num;
		}else{
			if(blk_start > -1){
				printf("found non zero %d bytes block at: %-llu -> %-llu\n",  block_size, blk_start, blk_num);
				blk_start = -1;
			}
		}
		blk_num += 1;
    }

	if(blk_start > -1){
		printf("found non zero %d bytes block at: %-llu -> %-llu\n",  block_size, blk_start, blk_num);
	}

    close(fd);
}

