
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

#ifndef CHROMAKEY_H
#define CHROMAKEY_H




#include "colorpicker.h"
#include "guicast.h"
#include "theme.h"
#include "loadbalance.h"
#include "pluginvclient.h"

#define RESET_DEFAULT_SETTINGS 10
#define RESET_ALL               0
#define RESET_RGB               1
#define RESET_SLOPE             2
#define RESET_THRESHOLD         3

#define MIN_VALUE   0.00
#define MAX_VALUE 100.00

class ChromaKey;
class ChromaKey;
class ChromaKeyWindow;
class ChromaKeyFText;
class ChromaKeyFSlider;
class ChromaKeyReset;
class ChromaKeyDefaultSettings;
class ChromaKeyClr;

class ChromaKeyConfig
{
public:
	ChromaKeyConfig();
	void reset(int clear);
	void copy_from(ChromaKeyConfig &src);
	int equivalent(ChromaKeyConfig &src);
	void interpolate(ChromaKeyConfig &prev,
		ChromaKeyConfig &next,
		int64_t prev_frame,
		int64_t next_frame,
		int64_t current_frame);
	int get_color();

	float red;
	float green;
	float blue;
	float threshold;
	float slope;
	int use_value;
};

class ChromaKeyColor : public BC_GenericButton
{
public:
	ChromaKeyColor(ChromaKey *plugin,
		ChromaKeyWindow *gui,
		int x,
		int y);

	int handle_event();

	ChromaKey *plugin;
	ChromaKeyWindow *gui;
};

class ChromaKeyThreshold : public BC_FSlider
{
public:
	ChromaKeyThreshold(ChromaKey *plugin, int x, int y);
	int handle_event();
	ChromaKey *plugin;
};
class ChromaKeySlope : public BC_FSlider
{
public:
	ChromaKeySlope(ChromaKey *plugin, int x, int y);
	int handle_event();
	ChromaKey *plugin;
};
class ChromaKeyUseValue : public BC_CheckBox
{
public:
	ChromaKeyUseValue(ChromaKey *plugin, int x, int y);
	int handle_event();
	ChromaKey *plugin;
};

class ChromaKeyFText : public BC_TumbleTextBox
{
public:
	ChromaKeyFText(ChromaKey *plugin, ChromaKeyWindow *gui,
		ChromaKeyFSlider *slider, float *output, int x, int y, float min, float max);
	~ChromaKeyFText();
	int handle_event();
	ChromaKey *plugin;
	ChromaKeyWindow *gui;
	ChromaKeyFSlider *slider;
	float *output;
	float min, max;
};

class ChromaKeyFSlider : public BC_FSlider
{
public:
	ChromaKeyFSlider(ChromaKey *plugin,
		ChromaKeyFText *text, float *output, int x, int y,
		float min, float max, int w);
	~ChromaKeyFSlider();
	int handle_event();
	ChromaKey *plugin;
	ChromaKeyFText *text;
	float *output;
};

class ChromaKeyClr : public BC_Button
{
public:
	ChromaKeyClr(ChromaKey *plugin, ChromaKeyWindow *gui, int x, int y, int clear);
	~ChromaKeyClr();
	int handle_event();
	ChromaKey *plugin;
	ChromaKeyWindow *gui;
	int clear;
};

class ChromaKeyReset : public BC_GenericButton
{
public:
	ChromaKeyReset(ChromaKey *plugin, ChromaKeyWindow *gui, int x, int y);
	int handle_event();
	ChromaKey *plugin;
	ChromaKeyWindow *gui;
};

class ChromaKeyDefaultSettings : public BC_GenericButton
{
public:
	ChromaKeyDefaultSettings(ChromaKey *plugin, ChromaKeyWindow *gui, int x, int y, int w);
	~ChromaKeyDefaultSettings();
	int handle_event();
	ChromaKey *plugin;
	ChromaKeyWindow *gui;
};

class ChromaKeyUseColorPicker : public BC_GenericButton
{
public:
	ChromaKeyUseColorPicker(ChromaKey *plugin, ChromaKeyWindow *gui, int x, int y);
	int handle_event();
	ChromaKey *plugin;
	ChromaKeyWindow *gui;
};


class ChromaKeyColorThread : public ColorPicker
{
public:
	ChromaKeyColorThread(ChromaKey *plugin, ChromaKeyWindow *gui);
	int handle_new_color(int output, int alpha);
	ChromaKey *plugin;
	ChromaKeyWindow *gui;
};


class ChromaKeyWindow : public PluginClientWindow
{
public:
	ChromaKeyWindow(ChromaKey *plugin);
	~ChromaKeyWindow();

	void create_objects();
	void update_gui(int clear);
	void update_sample();
	void done_event(int result);

	ChromaKeyColor *color;

	ChromaKeyThreshold *threshold;
	ChromaKeyFText *threshold_text;
	ChromaKeyFSlider *threshold_slider;
	ChromaKeyClr *threshold_Clr;

	ChromaKeyUseValue *use_value;
	ChromaKeyUseColorPicker *use_colorpicker;

	ChromaKeySlope *slope;
	ChromaKeyFText *slope_text;
	ChromaKeyFSlider *slope_slider;
	ChromaKeyClr *slope_Clr;

	ChromaKeyReset *reset;
	ChromaKeyDefaultSettings *default_settings;
	BC_SubWindow *sample;
	ChromaKey *plugin;
	ChromaKeyColorThread *color_thread;
};





class ChromaKeyServer : public LoadServer
{
public:
	ChromaKeyServer(ChromaKey *plugin);
	void init_packages();
	LoadClient* new_client();
	LoadPackage* new_package();
	ChromaKey *plugin;
};

class ChromaKeyPackage : public LoadPackage
{
public:
	ChromaKeyPackage();
	int y1, y2;
};

class ChromaKeyUnit : public LoadClient
{
public:
	ChromaKeyUnit(ChromaKey *plugin, ChromaKeyServer *server);
	void process_package(LoadPackage *package);
	ChromaKey *plugin;
};




class ChromaKey : public PluginVClient
{
public:
	ChromaKey(PluginServer *server);
	~ChromaKey();

	PLUGIN_CLASS_MEMBERS(ChromaKeyConfig)
	int process_buffer(VFrame *frame,
		int64_t start_position,
		double frame_rate);
	int handle_opengl();
	int is_realtime();
	void save_data(KeyFrame *keyframe);
	void read_data(KeyFrame *keyframe);
	void update_gui();

	VFrame *input, *output;
	ChromaKeyServer *engine;
};








#endif







