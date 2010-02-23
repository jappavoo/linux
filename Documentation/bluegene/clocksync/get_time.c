#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <sys/time.h>

int main(int argc, char *argv[])
{
    int rc;
    cpu_set_t cpus;
    struct timeval tv;
    unsigned int target_usec, tbu, tbl, tbc;

    CPU_ZERO(&cpus);
    CPU_SET(0, &cpus);

    rc = sched_setaffinity(0, sizeof(cpus), &cpus);
    if (rc != 0) {
	perror("sched_setaffinity failed");
	exit(-1);
    }

    do {
	do {
	    gettimeofday(&tv, NULL);
	} while (tv.tv_usec >= 990000);

	target_usec = ((tv.tv_usec / 1000) * 1000) + 2000;

	do {
	    gettimeofday(&tv, NULL);
            do {
		asm("mftbu %0; mftbl %1; mftbu %2" : "=r" (tbu), "=r" (tbl), "=r" (tbc) :);
            } while (tbu != tbc);
	} while (tv.tv_usec < target_usec);
    } while (tv.tv_usec != target_usec);

    printf("%lu %lu %u %u\n", tv.tv_sec, tv.tv_usec, tbu, tbl);

    return 0;
}
