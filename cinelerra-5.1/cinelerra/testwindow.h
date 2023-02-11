/*
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

#ifndef __TESTWINDOW_H__
#define __TESTWINDOW_H__

#include "bcwindow.h"
#include "bcsignals.h"
#include "bccolors.h"
#include "fonts.h"
#include "thread.h"

class TestWindowGUI : public BC_Window
{
	int tx, ty;
	int last_x, last_y;
	int in_motion;
public:
	int button_press_event();
	int button_release_event();
	int cursor_motion_event();
	TestWindowGUI();
	~TestWindowGUI();
};


class TestWindow : public Thread
{
	TestWindowGUI *gui;
public:
	TestWindow() : Thread(1,0,0) { gui = new TestWindowGUI(); start(); }
	~TestWindow() { delete gui; }
	void run() { gui->run_window(); }
};

#endif
