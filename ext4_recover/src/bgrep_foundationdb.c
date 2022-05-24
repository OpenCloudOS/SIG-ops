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

#define BLOCK_SIZE 4096
#define MAX_VALID_BLOCKS 131072
#define RECOVER_DIR "./RECOVER"

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <device_path> [max_file_size_in_bytes]\n", argv[0]);
        exit(1);
    }

	setlinebuf(stdout);

	int max_file_size = 0;
	if (argc == 3 ){
		max_file_size = atoi(argv[2]);
		printf("max file size: %d\n", max_file_size);
	}

    printf("creating recover directory '%s'\n", RECOVER_DIR);
    if (mkdir(RECOVER_DIR, 0700) < 0) {
        perror("failed to create recover dir:");
        return 1;
    }
    // .sqlite
    char *m1_start = "FoundationDB100\0";
	char *m1_suffix = ".sqlite";
    int m1_len = 16;
	// processId
	char *m2_start = "\x01\x00\x01\x70\xa5\x00\xdb\x0f";
	char *m2_suffix = ".processId";
	int m2_len = 8;
	//.sqlite-wal
	char *m3_start = "\x37\x7f\x06\x82\x00\x2d\xe2\x18\x00\x00\x10";
	char *m3_suffix = ".sqlite-wal";
	int m3_len = 11;
	//.db: reeal sqlite v3
	char *m4_start = "SQLite format 3\0";
	char *m4_suffix = ".sqlite.db";
    int m4_len = 16; 

    char *f = argv[1];
    printf("recovering from %s\n", f);

    int fd = open(f, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    size_t nread, blk_number=0;
    char buf[BLOCK_SIZE];
    char wfilename[100];
    int wfd = 0;
    char *p;
    char *pend;
    int started = 0;
    size_t nblocks = 0;
	
	uint64_t file_size = 0;
    while (nread = read(fd, buf, BLOCK_SIZE)) {
        if (!started) {
            if ( (p = memmem(buf, nread, m1_start, m1_len)) == buf) {
                sprintf(wfilename, "%s/%lu%s", RECOVER_DIR, blk_number, m1_suffix);
                file_size = max_file_size;
            }
			else if ( (p = memmem(buf, nread, m2_start, m2_len)) == buf) {
                sprintf(wfilename, "%s/%lu%s", RECOVER_DIR, blk_number, m2_suffix);
                file_size = max_file_size;
            }
			else if ( (p = memmem(buf, nread, m3_start, m3_len)) == buf) {
                sprintf(wfilename, "%s/%lu%s", RECOVER_DIR, blk_number, m3_suffix);
                file_size = max_file_size;
            }
			else if ( (p = memmem(buf, nread, m4_start, m4_len)) == buf) {
                sprintf(wfilename, "%s/%lu%s", RECOVER_DIR, blk_number, m4_suffix);
				uint16_t page_size;
				uint32_t page_count;
				page_size = __bswap_16(*(uint16_t*)(p+16));
                page_count = __bswap_32(*(uint32_t*)(p+28));
                file_size = (uint64_t)page_count * page_size;
                printf("page size %u, page count:%u, file size:%u\n", page_size, page_count, file_size);
                if( max_file_size > 0 && file_size > max_file_size){
                    file_size = max_file_size;
                    printf("truncate file to max_file_size:%d\n", max_file_size);
                }
            }

            if (p == buf) {
                started = 1;
                nblocks = 0;
                printf("found match file begin at block:%-10lu\n", blk_number);
                printf("dump file to %s... begin\n", wfilename);
                fflush(stdout);
                wfd = open(wfilename, O_WRONLY | O_CREAT | O_TRUNC, 0600);
                if (wfd < 0) {
                    perror("open");
                    return 1;
                }
            }
        }

        if (started) {
			if((nblocks+1)*BLOCK_SIZE >= file_size){
				size_t to_write = file_size - nblocks*BLOCK_SIZE;
                write(wfd, buf, to_write);
				printf("nwrite: %lu >= %lu: to_wirte:%lu\n", (nblocks+1)*BLOCK_SIZE, file_size, to_write);
                printf("file end at block:%-10lu\n", blk_number);
                close(wfd);
                started = 0;
            } else {
                write(wfd, buf, nread);
                nblocks += 1;
            }
        }
        blk_number += 1;
    }
    close(fd);
}

