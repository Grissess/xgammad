#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <X11/Xlib.h>
#include <X11/extensions/xf86vmode.h>

#include "coltbl.h"
#include "icct.h"

#ifndef DEF_CH_TIME
/* Multiply by WAIT_NSEC to get real time to change */
#define DEF_CH_TIME 50
#endif

#ifndef DEF_UPDATE_TIME
#define DEF_UPDATE_TIME 10
#endif

#ifndef WAIT_NSEC
/* 0.1 sec */
#define WAIT_NSEC 100000000
#endif

#ifndef DEF_PIPE_PATH
#define DEF_PIPE_PATH "/tmp/xgammad.%s"
#endif

#define gen_lin_interp(old, new, u, ulo, uhi) ((new) * ((u) - (ulo)) / ((uhi) - (ulo)) + (old) * ((uhi) - (u)) / ((uhi) - (ulo)))
#define clamp(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

int main(int argc, char **argv) {
	Display *dpy;
	int gammasz, i, j;
	unsigned short *buffer;
	char *pipetmpl = DEF_PIPE_PATH, pipepath[256], *uname = NULL, cmdbuf[256], *nextcmd;
	uid_t uid = getuid();
	unsigned nscreens;
	ssize_t cnt;
	double kelvin, ciex, ciey;
	long temp;
	struct passwd *pwent;

	/* State variables */
	int pipefd, pipewrfd;
	int running = 1;
	unsigned ticks = 0, update = DEF_UPDATE_TIME, chtime = DEF_CH_TIME, chinit = 0, chfini = chtime;
	double rpeak = 1, gpeak = 1, bpeak = 1;
	double oldrpeak = 1, oldgpeak = 1, oldbpeak = 1;
	double rgamma = 1, ggamma = 1, bgamma = 1;
	double oldrgamma = 1, oldggamma = 1, oldbgamma = 1;
	struct timespec now, remain;

	while(pwent = getpwent()) {
		if(pwent->pw_uid == uid) {
			uname = pwent->pw_name;
			break;
		}
	}

	if(!uname) {
		printf("WARNING: Unable to discern username, using itoa uid\n");
		uname = malloc(16);
		snprintf(uname, 16, "%d", uid);
	}

	snprintf(pipepath, sizeof(pipepath), pipetmpl, uname);

	dpy = XOpenDisplay(NULL);
	if(!dpy) {
		printf("Failed to open display\n");
		return 1;
	}

	nscreens = XScreenCount(dpy);
	printf("Display has %u screens\n", nscreens);

	XF86VidModeGetGammaRampSize(dpy, 0, &gammasz);
	printf("Gamma Ramp size is %d\n", gammasz);

	buffer = malloc(sizeof(*buffer) * gammasz * 3);
	if(!buffer) {
		printf("Failed to malloc gamma buffer\n");
		return 1;
	}

	if(unlink(pipepath) < 0) {
		if(errno != ENOENT) {
			perror("unlink pipe");
			return 1;
		}
	}

	if(mkfifo(pipepath, 0600) < 0) {
		perror("mkfifo");
		return 1;
	}

	if((pipefd = open(pipepath, O_RDONLY | O_NONBLOCK)) < 0) {
		perror("open fifo");
		return 1;
	}

	if((pipewrfd = open(pipepath, O_WRONLY | O_NONBLOCK)) < 0) {
		perror("open fifo for write");
		return 1;
	}

	for(i = 0; i < gammasz; i++) {
		buffer[i] = i << 8 | i;
		buffer[i + gammasz] = buffer[i];
		buffer[i + gammasz * 2] = buffer[i];
	}

	while(running) {
		/* Update values */
		if(ticks <= chfini && ticks > chinit) {
			colortable(buffer, gammasz, gen_lin_interp(oldrpeak, rpeak, ticks, chinit, chfini), gen_lin_interp(oldrgamma, rgamma, ticks, chinit, chfini));
			colortable(buffer + gammasz, gammasz, gen_lin_interp(oldgpeak, gpeak, ticks, chinit, chfini), gen_lin_interp(oldggamma, ggamma, ticks, chinit, chfini));
			colortable(buffer + gammasz * 2, gammasz, gen_lin_interp(oldbpeak, bpeak, ticks, chinit, chfini), gen_lin_interp(oldbgamma, bgamma, ticks, chinit, chfini));
			for(i = 0; i < nscreens; i++)
				XF86VidModeSetGammaRamp(dpy, i, gammasz, buffer, buffer + gammasz, buffer + gammasz * 2);
		} else if(ticks % update == 0) {
			for(i = 0; i < nscreens; i++)
				XF86VidModeSetGammaRamp(dpy, i, gammasz, buffer, buffer + gammasz, buffer + gammasz * 2);
		}

		/* Process any commands */
		cnt = read(pipefd, cmdbuf, sizeof(cmdbuf));
		if(cnt == 0) {  /* EOF */
			running = 0;
		} else if(cnt < 0) {
			if(errno != EAGAIN || errno != EWOULDBLOCK) {
				perror("read");
				return 1;
			}
		}
		for(i = 0; i < cnt; i++) {
			printf("CMD: %c\n", cmdbuf[i]);
			switch(cmdbuf[i]) {
				case 'k':  /* Set Kelvin */
					kelvin = strtod(cmdbuf + (i + 1), &nextcmd);
					if(*nextcmd == '\n') {
						oldrpeak = rpeak;
						oldgpeak = gpeak;
						oldbpeak = bpeak;
						kelvin2ciexy(kelvin, &ciex, &ciey);
						ciexyy2srgb(ciex, ciey, 1, &rpeak, &gpeak, &bpeak);
						rpeak = clamp(rpeak, 0, 1);
						gpeak = clamp(gpeak, 0, 1);
						bpeak = clamp(bpeak, 0, 1);
						chinit = ticks;
						chfini = ticks + chtime;
						printf("\tKelvin to %g\n\tNew peaks to %g,%g,%g\n", kelvin, rpeak, gpeak, bpeak);
					}
					i = (nextcmd - cmdbuf);
					break;

				case 'r':  /* Reset */
					oldrpeak = rpeak;
					oldgpeak = gpeak;
					oldbpeak = bpeak;
					rpeak = 1;
					gpeak = 1;
					bpeak = 1;
					chinit = ticks;
					chfini = ticks + chtime;
					printf("\tReset all curves to unity\n");
					break;

				case 't':  /* Set change time */
					temp = strtol(cmdbuf + (i + 1), &nextcmd, 10);
					if(*nextcmd == '\n') {
						chtime = temp;
						if(ticks < chfini) {
							chfini = chinit + chtime;
						}
						printf("\tChange time to %u\n", chtime);
					}
					i = (nextcmd - cmdbuf);
					break;

				case 'c':  /* Set refresh time */
					temp = strtol(cmdbuf + (i + 1), &nextcmd, 10);
					if(*nextcmd == '\n') {
						update = temp;
						printf("\tRefresh time to %u\n", update);
					}
					i = (nextcmd - cmdbuf);
					break;

				case 'u':  /* Update right now */
					chfini = ticks;
					colortable(buffer, gammasz, rpeak, rgamma);
					colortable(buffer + gammasz, gammasz, gpeak, ggamma);
					colortable(buffer + gammasz * 2, gammasz, bpeak, bgamma);
					for(i = 0; i < nscreens; i++)
						XF86VidModeSetGammaRamp(dpy, i, gammasz, buffer, buffer + gammasz, buffer + gammasz * 2);
					printf("\tUpdate forced\n");
					break;

				case 'g':  /* Gamma */
					switch(cmdbuf[i + 1]) {
						case 'r':
							temp = strtod(cmdbuf + (i + 2), &nextcmd);
							if(*nextcmd == '\n') {
								rgamma = temp;
								chinit = ticks;
								chfini = ticks + chtime;
								printf("Gamma red set to %g\n", rgamma);
							}
							i = (nextcmd - cmdbuf);
							break;

						case 'g':
							temp = strtod(cmdbuf + (i + 2), &nextcmd);
							if(*nextcmd == '\n') {
								ggamma = temp;
								chinit = ticks;
								chfini = ticks + chtime;
								printf("Gamma green set to %g\n", ggamma);
							}
							i = (nextcmd - cmdbuf);
							break;

						case 'b':
							temp = strtod(cmdbuf + (i + 2), &nextcmd);
							if(*nextcmd == '\n') {
								bgamma = temp;
								chinit = ticks;
								chfini = ticks + chtime;
								printf("Gamma blue set to %g\n", bgamma);
							}
							i = (nextcmd - cmdbuf);
							break;

						default:
							temp = strtod(cmdbuf + (i + 1), &nextcmd);
							if(*nextcmd == '\n') {
								rgamma = ggamma = bgamma = temp;
								chinit = ticks;
								chfini = ticks + chtime;
								printf("All gamma set to %g\n", rgamma);
							}
							i = (nextcmd - cmdbuf);
							break;
					}
					break;

				case 'q':  /* Quit */
					printf("\tQuit\n");
					close(pipewrfd);
					break;

				case 'd':  /* Debug */
					printf("\tCurrent peak values: %g %g %g\n\tCurrent gamma values: %g %g %g\n", rpeak, gpeak, bpeak, rgamma, ggamma, bgamma);
					printf("\tCurrent buffer status:\nRED:\n");
					for(j = 0; j < gammasz; j++) {
						printf("%hu ", buffer[j]);
					}
					printf("\nGREEN:\n");
					for(j = 0; j < gammasz; j++) {
						printf("%hu ", buffer[j + gammasz]);
					}
					printf("\nBLUE:\n");
					for(j = 0; j < gammasz; j++) {
						printf("%hu ", buffer[j + gammasz * 2]);
					}
					printf("\n");
					break;
			}
		}

		/* Next tick */
		ticks++;
		printf("\r                                         \rTICK: %u CHINIT: %u CHFINI: %u PROGRESS: %f", ticks, chinit, chfini, (double) (ticks - chinit) / (chfini - chinit));
		fflush(stdout);
		clock_gettime(CLOCK_MONOTONIC, &now);
		now.tv_nsec += WAIT_NSEC;
		while(now.tv_nsec >= 1000000000) {
			now.tv_sec++;
			now.tv_nsec -= 1000000000;
		}
		while(clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &now, &remain) < 0)
			;
	}

	printf("\nShutting down.\n");
	close(pipefd);
	unlink(pipepath);

	XCloseDisplay(dpy);
	return 0;
}
