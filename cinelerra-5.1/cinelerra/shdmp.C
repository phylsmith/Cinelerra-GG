/*
 * CINELERRA
 * Copyright (C) 2016-2020 William Morrow
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <error.h>
#include <signal.h>
#include <linux/input.h>

input_event ev;

int main(int ac, char **av)
{
	setbuf(stdout, 0);
	if( ac < 2 ) { printf("usage: %s /dev/input/by-id/<device?>\n", av[0]); exit(1); }
	int fd = open(av[1], O_RDONLY);
	if( !fd ) { perror(av[1]); exit(1); }

	for(;;) {
		int ret = read(fd, &ev, sizeof(ev));
		if( ret != sizeof(ev) ) {
			if( ret < 0 ) perror("read event");
			printf("bad read: %d\n", ret);
			break;
		}
		printf("event: (%d, %d, 0x%x)\n", ev.type, ev.code, ev.value);
	}

	return 0;
}

