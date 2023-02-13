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

#ifndef __DRAGCHECKBOX_H__
#define __DRAGCHECKBOX_H__

#include "bctoggle.h"
#include "mwindow.inc"
#include "track.inc"

class DragCheckBox : public BC_CheckBox
{
public:
	DragCheckBox(MWindow *mwindow, int x, int y, const char *text, int *value,
		float drag_x, float drag_y, float drag_w, float drag_h);
	~DragCheckBox();
	virtual Track *get_drag_track() = 0;
	virtual int64_t get_drag_position() = 0;
	virtual void update_gui() { return; };
	void create_objects();
	int get_track_w();
	int get_track_h();
	void bound();
	static void draw_boundary(VFrame *out, int x, int y, int w, int h);

	int check_pending();
	int drag_activate();
	void drag_deactivate();

	int handle_event();
	int grab_event(XEvent *event);
	int handle_ungrab();
	
	MWindow *mwindow;
	int grabbed, dragging, pending;
	float drag_x, drag_y;
	float drag_w, drag_h;
	float drag_dx, drag_dy;
};

#endif

