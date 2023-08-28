
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

/*
 * 2023. Derivative by Flip plugin.
*/

#ifndef MIRRORWINDOW_H
#define MIRRORWINDOW_H


class MirrorThread;
class MirrorWindow;
class MirrorToggle;
class MirrorSwapSide;
class MirrorReflectionCenter;
class MirrorReflectionCenterXText;
class MirrorReflectionCenterXSlider;
class MirrorReflectionCenterYText;
class MirrorReflectionCenterYSlider;
class MirrorReflectionCenterClr;
class MirrorReset;



#include "filexml.inc"
#include "mirror.h"
#include "mutex.h"
#include "pluginvclient.h"

#define RESET_DEFAULT_SETTINGS 10
#define RESET_ALL    0
#define RESET_REFLECTION_CENTER_X 1
#define RESET_REFLECTION_CENTER_Y 2


class MirrorWindow : public PluginClientWindow
{
public:
	MirrorWindow(MirrorMain *client);
	~MirrorWindow();

	void create_objects();
	void update(int clear);

	void update_reflection_center_enable();

	MirrorMain *client;
	MirrorToggle *mirror_vertical;
	MirrorToggle *mirror_horizontal;
        MirrorSwapSide *mirror_swapvertical;
        MirrorSwapSide *mirror_swaphorizontal;
	MirrorReflectionCenter *reflection_center_enable;

	// Mirror Reflection Center X, Y
	MirrorReflectionCenterXText *reflection_center_x_text;
	MirrorReflectionCenterXSlider *reflection_center_x_slider;
	MirrorReflectionCenterClr *reflection_center_x_clr;

	MirrorReflectionCenterYText *reflection_center_y_text;
	MirrorReflectionCenterYSlider *reflection_center_y_slider;
	MirrorReflectionCenterClr *reflection_center_y_clr;

	MirrorReset *reset;
};

class MirrorToggle : public BC_CheckBox
{
public:
	MirrorToggle(MirrorMain *client, int *output, char *string, int x, int y);
	~MirrorToggle();
	int handle_event();

	MirrorMain *client;
	int *output;
};

class MirrorSwapSide : public BC_CheckBox
{
public:
	MirrorSwapSide(MirrorMain *client, int *output, char *string, int x, int y);
	~MirrorSwapSide();
	int handle_event();

	MirrorMain *client;
	int *output;
};

class MirrorReflectionCenter : public BC_CheckBox
{
public:
	MirrorReflectionCenter(MirrorWindow *window, MirrorMain *client, int x, int y);
	~MirrorReflectionCenter();
	int handle_event();

	MirrorMain *client;
	MirrorWindow *window;
};

class MirrorReflectionCenterXText : public BC_TumbleTextBox
{
public:
	MirrorReflectionCenterXText(MirrorWindow *window,
		MirrorMain *client,
		int x,
		int y);
	~MirrorReflectionCenterXText();
	int handle_event();
	MirrorMain *client;
	MirrorWindow *window;
};

class MirrorReflectionCenterXSlider : public BC_FSlider
{
public:
	MirrorReflectionCenterXSlider(MirrorWindow *window, MirrorMain *client,
		int x, int y, int w);
	~MirrorReflectionCenterXSlider();
	int handle_event();
	MirrorWindow *window;
	MirrorMain *client;
};

class MirrorReflectionCenterYText : public BC_TumbleTextBox
{
public:
	MirrorReflectionCenterYText(MirrorWindow *window,
		MirrorMain *client,
		int x,
		int y);
	~MirrorReflectionCenterYText();
	int handle_event();
	MirrorMain *client;
	MirrorWindow *window;
};

class MirrorReflectionCenterYSlider : public BC_FSlider
{
public:
	MirrorReflectionCenterYSlider(MirrorWindow *window, MirrorMain *client,
		int x, int y, int w);
	~MirrorReflectionCenterYSlider();
	int handle_event();
	MirrorWindow *window;
	MirrorMain *client;
};

class MirrorReflectionCenterClr : public BC_Button
{
public:
	MirrorReflectionCenterClr(MirrorWindow *window, MirrorMain *client,
		int x, int y, int clear);
	~MirrorReflectionCenterClr();
	int handle_event();
	MirrorWindow *window;
	MirrorMain *client;
	int clear;
};

class MirrorReset : public BC_GenericButton
{
public:
	MirrorReset(MirrorMain *client, MirrorWindow *window, int x, int y);
	~MirrorReset();
	int handle_event();
	MirrorMain *client;
	MirrorWindow *window;
};

#endif
