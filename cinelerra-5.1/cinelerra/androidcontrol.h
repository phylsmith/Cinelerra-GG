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

#ifndef __ANDROID_CONTROL_H__
#define __ANDROID_CONTROL_H__

#include "androidcontrol.inc"
#include "mwindow.inc"
#include "mwindowgui.inc"
#include "mutex.h"
#include "thread.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>


class AndroidControl : public Thread {
	static const unsigned short PORT = 23432;
	int done;
	int sockfd;
	char buf[256];
	char *msg;
	int msg_len;
	MWindowGUI *mwindow_gui;

	bool is_msg(const char *cp);
	void press(int key);
public:
	int open(unsigned short port);
	void close();
	void run();

	AndroidControl(MWindowGUI *mwindow_gui);
	~AndroidControl();
};

#endif
