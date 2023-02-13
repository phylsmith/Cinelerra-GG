/*
 * CINELERRA
 * Copyright (C) 2008 Adam Williams <broadcast at earthling dot net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef PANEDIVIDERS_H
#define PANEDIVIDERS_H

#include "guicast.h"
#include "mwindow.inc"

class PaneDivider : public BC_SubWindow
{
public:
	PaneDivider(MWindow *mwindow, int x, int y, int length, int is_x);
	~PaneDivider();
	void create_objects();
	void reposition_window(int x, int y, int length);
	int button_press_event();
	int button_release_event();
	int cursor_motion_event();
	int cursor_enter_event();
	int cursor_leave_event();
	void draw(int flush);

// set if it is a vertical line dividing horizontal panes
	int is_x;
	int button_down;
	int is_dragging;
	int origin_x;
	int origin_y;
	int status;
	MWindow *mwindow;
	BC_Pixmap *images[3];
};



#endif
