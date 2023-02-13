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

#ifndef _REMOTECONTROL_H_
#define _REMOTECONTROL_H_

#include "bcpopup.h"
#include "bcwindow.h"
#include "mutex.h"
#include "mwindowgui.inc"
#include "thread.h"

class RemoteControl;
class RemoteGUI;
class RemoteHandler;
class RemoteWindow;

class RemoteGUI : public BC_Popup
{
public:
	RemoteControl *remote_control;

	void set_active(RemoteHandler *handler);
	void set_inactive();
	void fill_color(int color);
	void tile_windows(int config);
	int button_press_event();
	int keypress_event();
	int process_key(int key);

	RemoteGUI(BC_WindowBase *wdw, RemoteControl *remote_control);
	~RemoteGUI();
};

class RemoteWindow : public BC_Window
{
public:
	RemoteControl *remote_control;

	 RemoteWindow(RemoteControl *remote_control);
	 ~RemoteWindow();
};

class RemoteHandler
{
public:
	RemoteGUI *gui;
	int color;

	void activate() { gui->set_active(this); }
	virtual int remote_key(int key);
	virtual int process_key(int key);

	virtual int is_keytv() { return 0; }
	virtual int is_wintv() { return 0; }
	virtual int is_x10tv() { return 0; }

	RemoteHandler(RemoteGUI *gui, int color);
	virtual ~RemoteHandler();
};

class RemoteControl : Thread {
public:
	MWindowGUI *mwindow_gui;
	RemoteWindow *remote_window;
	RemoteGUI *gui;
	RemoteHandler *handler;
	Mutex *active_lock;

	void run();
	void set_color(int color);
	void fill_color(int color);
	int remote_key(int key);

	int activate(RemoteHandler *handler=0);
	int deactivate();
	bool is_active() { return handler != 0; }

	RemoteControl(MWindowGUI *gui);
	~RemoteControl();
};

#endif
