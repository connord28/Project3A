#NAME: Connor Daly, Ryan Daly
#EMAIL: connord2838@gmail.com, ryand3031@ucla.edu
#ID: 305416912, 505416119

build: lab3a.c ext2_fs.h
	gcc -Wall -Wextra lab3a.c -o lab3a

dist: build README
	tar -czvf lab3a-305416912.tar.gz lab3a.c ext2_fs.h README Makefile

clean:
	rm -f lab3a-305416912.tar.gz lab3a