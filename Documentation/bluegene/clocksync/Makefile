all: get_time set_time

get_time: get_time.c
	gcc -O2 -Wall -o get_time get_time.c

set_time: set_time.c
	gcc -O2 -Wall -o set_time set_time.c

clean:
	rm -f get_time get_time.o
	rm -f set_time set_time.o
