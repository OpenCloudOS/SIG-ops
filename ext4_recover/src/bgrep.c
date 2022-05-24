#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define BUFLEN 4096
int main(int argc, char **argv){
	int needle_len;
	char *needle;
	char buf[BUFLEN];
	char * f;

	if(argc < 4){
		printf("Usage: %s file_path bytes_len bytes_str\n", argv[0]);
		exit(1);
	}
	setlinebuf(stdout);

	f = argv[1];
	needle_len = atoi(argv[2]);
	needle = argv[3];

	int fd = open(f, O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}
	printf("checkint match bytes from file %s\n", f);


	size_t nread, blk_num=0;
	ssize_t blk_start= -1; 
	char *p;
	while (nread = read(fd, buf, BUFLEN)) {
		if( (p = memmem(buf, nread, needle, needle_len)) != NULL){
			printf("found math at block %lu offset: %lu\n", blk_num, p-buf);
		}
		blk_num += 1;
	}   
}
