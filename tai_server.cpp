/*
 * tai_server.cpp
 * hexdump -ve '1/4 "%d\n"' /dev/acq400.0.wr_tai
 *
 *  Created on: 25 Nov 2022
 *      Author: pgm
 */


#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>

#define INTERVAL_US	100000		// 10Hz poll is a nice balance of load and responsiveness


void publish(unsigned t1)
{
	char cmd[128];
	snprintf(cmd, 128, "echo tai_date $(date -ud @%u) >/tmp/tai_date; mv /tmp/tai_date /etc/acq400/0/tai_date", t1);
	system(cmd);
}
void missed_alarm(int signum)
{

}

int main(int argc, char** argv)
{
        struct itimerval timer;
        sigset_t alarm_sig;
        int signum;
        unsigned int t1;
        unsigned int t0 = 0;

        FILE* fp = fopen("/dev/acq400.0.wr_tai", "r");
        if (fp == 0){
        	perror("/dev/acq400.0.wr_tai");
        	exit(errno);
        }
        sigemptyset(&alarm_sig);
        sigaddset(&alarm_sig, SIGALRM);
        signal(SIGALRM, missed_alarm);
        timer.it_value.tv_sec = timer.it_interval.tv_sec = 0;
        timer.it_value.tv_usec = timer.it_interval.tv_usec = INTERVAL_US;
	if (setitimer(ITIMER_REAL, &timer, 0) < 0) {
		perror("setitimer");
		exit(1);
	}

	int nb;

	while((nb = fread(&t1, sizeof(unsigned int), 1, fp)) > 0){
		if (t1 != t0){
			publish(t1);
			t0 = t1;
		}
		sigwait(&alarm_sig, &signum);
	}
}
