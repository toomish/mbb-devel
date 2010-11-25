/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include <termios.h>
#include <unistd.h>

#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>

#include "debug.h"

enum { TTY_ECHO_OFF, TTY_ECHO_ON };

static int tty_open(void)
{
	static int tty_fd = -1;

	if (tty_fd < 0) {
		tty_fd = open("/dev/tty", O_RDWR);
		if (tty_fd < 0)
			msg_err("open /dev/tty");
	}

	return tty_fd;
}

static void tty_echo(int echo)
{
	struct termios tio;
	int fd;

	if ((fd = tty_open()) < 0)
		return;

	if (tcgetattr(fd, &tio) < 0) {
		msg_err("tcgetattr");
		return;
	}

	if (echo == TTY_ECHO_ON) {
		tio.c_lflag |= ECHO;
		tio.c_lflag |= ICANON;
	} else if (echo == TTY_ECHO_OFF) {
		tio.c_lflag &= ~ECHO;
		tio.c_lflag &= ~ICANON;
	}

	if (tcsetattr(fd, TCSANOW, &tio) < 0)
		msg_err("tcsetattr");
}

int caution(void)
{
	int ch;

	printf("these actions will lead to irreparable consequences\n");
	printf("do you want to continue (Y/N) ?");
	fflush(stdout);

	tty_echo(TTY_ECHO_OFF);

	do {
		ch = tolower(getchar());
	} while (ch != 'y' && ch != 'n');

	tty_echo(TTY_ECHO_ON);

	printf(" %c\n", ch);

	return ch == 'y';
}

