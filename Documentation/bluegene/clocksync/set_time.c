#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <sys/time.h>

int main(int argc, char *argv[])
{
    int rc, i;
    cpu_set_t cpus;
    struct timeval tv;
    unsigned int sec, usec, tbu, tbl;
    unsigned long long us_0, us_now, us_target;
    unsigned long long tb_0, tb_now, tb_target;

    if (argc != 5) {
	fprintf(stderr, "Usage: %s <sec> <usec> <tbu> <tbl>\n", argv[0]);
	exit(-1);
    }

    sec = strtoul(argv[1], NULL, 0);
    usec = strtoul(argv[2], NULL, 0);
    tbu = strtoul(argv[3], NULL, 0);
    tbl = strtoul(argv[4], NULL, 0);

    us_0 = (sec * 1000000ull) + usec;
    tb_0 = (tbu * (1ull << 32)) + tbl;

    CPU_ZERO(&cpus);
    CPU_SET(0, &cpus);

    rc = sched_setaffinity(0, sizeof(cpus), &cpus);
    if (rc != 0) {
	perror("sched_setaffinity failed");
	exit(-1);
    }

    for (i = 0; i < 2; i++) { // do it twice to warm things up
	asm("mftbu %0; mftbl %1" : "=r" (tbu), "=r" (tbl) :);
	tb_now = (tbu * (1ull << 32)) + tbl;

	us_now = us_0 + ((tb_now - tb_0) / (850000000 / 1000000));

	us_target = us_now + 2000;
	tb_target = tb_0 + ((us_target - us_0) * (850000000 / 1000000));

	tv.tv_sec = us_target / 1000000;
	tv.tv_usec = us_target % 1000000;

	do {
	    asm("mftbu %0; mftbl %1" : "=r" (tbu), "=r" (tbl) :);
	} while (((tbu * (1ull << 32)) + tbl) < tb_target);

	rc = settimeofday(&tv, NULL);
	if (rc != 0) {
	    perror("settimeofday failed");
	    exit(-1);
	}
    }

    return 0;
}
