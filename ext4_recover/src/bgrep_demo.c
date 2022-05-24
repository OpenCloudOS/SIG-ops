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

#define BLOCK_SIZE 4096
#define MAX_VALID_BLOCKS 131072
#define RECOVER_DIR "./RECOVER"

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s file\n", argv[0]);
        exit(1);
    }
    printf("creating recover directory '%s'\n", RECOVER_DIR);
    if (mkdir(RECOVER_DIR, 0700) < 0) {
        perror("failed to create recover dir:");
        return 1;
    }
    // fdx
    char *match_fdx_start = "\x3f\xd7\x6c\x17\x1dLucene50StoredFieldsHighIndex";
    int match_fdx_len = 34;
    // fdt
    char *match_fdt1_start = "\x3f\xd7\x6c\x17\x1cLucene50StoredFieldsFastData";
    int match_fdt1_len = 33;
    char *match_fdt2_start = "\x3f\xd7\x6c\x17\x1cLucene50StoredFieldsHighData";
    int match_fdt2_len = 33;

    char *match_fnm_start = "\x3f\xd7\x6c\x17\x12Lucene60FieldInfos";
    int match_fnm_len = 23;
    char *match_si_start = "\x3f\xd7\x6c\x17\x13Lucene70SegmentInfo";
    int match_si_len = 24;
    char *match_end = "\xc0\x28\x93\xe8\x00\x00\x00\x00";
    char match_end_len = 8;
    char *f = argv[1];
    printf("recovering from %s\n", f);

    int fd = open(f, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    size_t nread, blk_number;
    char buf[BLOCK_SIZE];
    char wfilename[100];
    int wfd = 0;
    char *p;
    char *pend;
    int started = 0;
    size_t nblocks = 0;
    while (nread = read(fd, buf, BLOCK_SIZE)) {
        p = buf;
        if (!started) {
            if (((p = memmem(buf, nread, match_fdt1_start, match_fdt1_len)) == buf) ||
                ((p = memmem(buf, nread, match_fdt2_start, match_fdt2_len)) == buf)) {
                sprintf(wfilename, "%s/%lu.fdt", RECOVER_DIR, blk_number);
            }
            /*
            else if( (p=memmem(buf, nread, match_fdx_start, match_fdx_len)) == buf){
                sprintf(wfilename, "./recover/%lu.fdx", blk_number);
            }else if( (p=memmem(buf, nread, match_fnm_start, match_fnm_len)) == buf){
                sprintf(wfilename, "./recover/%lu.fnm", blk_number);
            }else if( (p=memmem(buf, nread, match_si_start, match_si_len)) == buf){
                sprintf(wfilename, "./recover/%lu.si", blk_number);
            }
            */

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
            pend = memmem(buf, nread, match_end, match_end_len);
            // found end str
            if (pend != NULL) {
                pend = pend + match_end_len + 8;
                write(wfd, buf, pend - buf);
                printf("found match end at block:%-10lu\n", blk_number);
                close(wfd);
                started = 0;
            } else if (nblocks > MAX_VALID_BLOCKS) {
                write(wfd, buf, nread);
                printf("reach MAX_VALID_BLOCKS at block:%-10lu\n", blk_number);
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

