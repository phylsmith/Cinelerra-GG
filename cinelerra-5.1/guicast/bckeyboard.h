/*
 * CINELERRA
 * Copyright (C) 2020 William Morrow
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


#ifndef _BC_KEYBOARD_H_
#define _BC_KEYBOARD_H_

#include "bcwindowbase.inc"
#include "bcsignals.inc"
#include "mutex.h"

// global listeners for remote control

class BC_KeyboardHandler {
	int(BC_WindowBase::*handler)(BC_WindowBase *win);
	BC_WindowBase *win;
	static ArrayList<BC_KeyboardHandler*> listeners;
	friend class BC_WindowBase;
	friend class BC_Signals;
public:
	BC_KeyboardHandler(int(BC_WindowBase::*h)(BC_WindowBase *), BC_WindowBase *d)
		: handler(h), win(d) {}
	~BC_KeyboardHandler() {}
	static int run_listeners(BC_WindowBase *wp);
	int run_event(BC_WindowBase *wp);
	static void kill_grabs();
};

class BC_KeyboardHandlerLock {
	static Mutex keyboard_listener_mutex;
public:
	BC_KeyboardHandlerLock() { keyboard_listener_mutex.lock(); }
	~BC_KeyboardHandlerLock() { keyboard_listener_mutex.unlock(); }
};

#endif
