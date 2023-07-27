
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

#ifndef GAINWINDOW_H
#define GAINWINDOW_H

#define TOTAL_LOADS 5
#define GAINLEVEL_MAX 40

class GainThread;
class GainWindow;
class GainLevelText;
class GainLevelSlider;
class GainLevelClr;

#include "filexml.h"
#include "gain.h"
#include "guicast.h"
#include "mutex.h"
#include "pluginclient.h"



class GainWindow : public PluginClientWindow
{
public:
	GainWindow(GainMain *plugin);
	~GainWindow();

	void create_objects();
        void update();

	GainMain *plugin;
	GainLevelText *level_text;
	GainLevelSlider *level_slider;
	GainLevelClr *level_clr;
};

class GainLevelText : public BC_TumbleTextBox
{
public:
	GainLevelText(GainWindow *window, GainMain *plugin, int x, int y);
	~GainLevelText();
	int handle_event();

	GainWindow *window;
	GainMain *plugin;
};

class GainLevelSlider : public BC_FSlider
{
public:
	GainLevelSlider(GainWindow *window, GainMain *plugin, int x, int y);
	~GainLevelSlider();
	int handle_event();

	GainWindow *window;
	GainMain *plugin;
};

class GainLevelClr : public BC_Button
{
public:
	GainLevelClr(GainWindow *window, GainMain *plugin, int x, int y);
	~GainLevelClr();
	int handle_event();

	GainWindow *window;
	GainMain *plugin;
};




#endif
