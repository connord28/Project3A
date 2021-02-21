build: lab3a.c ext2_fs.h
	gcc -Wall -Wextra lab3a.c -o lab3a

raw: lab3a.c ext2_fs.h
	gcc lab3a.c -o lab3a

dist: build README
	tar -czvf lab3a-305416912.tar.gz lab3a.c ext2_fs.h README Makefile

clean:
	rm -f lab3a-305416912.tar.gz lab3a