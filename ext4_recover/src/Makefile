all: ext4recover bgrep_demo

ext4recover: ext4recover.c
	$(CC) -Werror -o $@ $< -lcom_err -lext2fs 

bgrep_%: bgrep_%.c
	$(CC) -Werror -o $@ $<

clean:
	rm -f ext4recover bgrep_demo bgrep_non_zero
