build: lab3a.c ext2_fs.h
	gcc -Wall -Wextra lab3a.c -o lab3a

raw: lab3a.c ext2_fs.h
	gcc lab3a.c -o lab3a
