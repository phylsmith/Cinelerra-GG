
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

#define RESET_DEFAULT_SETTINGS 20
#define RESET_ALL               0
#define RESET_RGB               1
#define RESET_MIN_BRIGHTNESS    2
#define RESET_MAX_BRIGHTNESS    3
#define RESET_TOLERANCE         4
#define RESET_SATURATION        5
#define RESET_MIN_SATURATION    6
#define RESET_IN_SLOPE          7
#define RESET_OUT_SLOPE         8
#define RESET_ALPHA_OFFSET      9
#define RESET_SPILL_THRESHOLD  10
#define RESET_SPILL_AMOUNT     11

#define MIN_VALUE   0.00
#define MAX_VALUE 100.00
#define MIN_SLOPE   0.00
#define MAX_SLOPE  20.00
#define MAX_ALPHA 100.00

class ChromaKeyHSV;
class ChromaKeyWindow;
class ChromaKeyFText;
class ChromaKeyFSlider;
class ChromaKeyReset;
class ChromaKeyClr;

enum {
	CHROMAKEY_POSTPROCESS_NONE,
	CHROMAKEY_POSTPROCESS_BLUR,
	CHROMAKEY_POSTPROCESS_DILATE
};

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

	// Output mode
	bool  show_mask;
	// Key color definition
	float red;
	float green;
	float blue;
	// Key shade definition
	float min_brightness;
	float max_brightness;
	float saturation;
	float min_saturation;
	float tolerance;
	// Mask feathering
	float in_slope;
	float out_slope;
	float alpha_offset;
	// Spill light compensation
	float spill_threshold;
	float spill_amount;
};

class ChromaKeyColor : public BC_GenericButton
{
public:
	ChromaKeyColor(ChromaKeyHSV *plugin,
		ChromaKeyWindow *gui,
		int x,
		int y);

	int handle_event();

	ChromaKeyHSV *plugin;
	ChromaKeyWindow *gui;
};

class ChromaKeyReset : public BC_GenericButton
{
public:
	ChromaKeyReset(ChromaKeyHSV *plugin, ChromaKeyWindow *gui, int x, int y);
	int handle_event();
	ChromaKeyHSV *plugin;
	ChromaKeyWindow *gui;
};

class ChromaKeyFText : public BC_TumbleTextBox
{
public:
	ChromaKeyFText(ChromaKeyHSV *plugin, ChromaKeyWindow *gui,
		ChromaKeyFSlider *slider, float *output, int x, int y, float min, float max);
	~ChromaKeyFText();
	int handle_event();
	ChromaKeyHSV *plugin;
	ChromaKeyWindow *gui;
	ChromaKeyFSlider *slider;
	float *output;
	float min, max;
};

class ChromaKeyFSlider : public BC_FSlider
{
public:
	ChromaKeyFSlider(ChromaKeyHSV *plugin,
		ChromaKeyFText *text, float *output, int x, int y,
		float min, float max, int w);
	~ChromaKeyFSlider();
	int handle_event();
	ChromaKeyHSV *plugin;
	ChromaKeyFText *text;
	float *output;
};

class ChromaKeyClr : public BC_Button
{
public:
	ChromaKeyClr(ChromaKeyHSV *plugin, ChromaKeyWindow *gui, int x, int y, int clear);
	~ChromaKeyClr();
	int handle_event();
	ChromaKeyHSV *plugin;
	ChromaKeyWindow *gui;
	int clear;
};


class ChromaKeyUseColorPicker : public BC_GenericButton
{
public:
	ChromaKeyUseColorPicker(ChromaKeyHSV *plugin, ChromaKeyWindow *gui, int x, int y);
	int handle_event();
	ChromaKeyHSV *plugin;
	ChromaKeyWindow *gui;
};


class ChromaKeyColorThread : public ColorPicker
{
public:
	ChromaKeyColorThread(ChromaKeyHSV *plugin, ChromaKeyWindow *gui);
	int handle_new_color(int output, int alpha);
	ChromaKeyHSV *plugin;
	ChromaKeyWindow *gui;
};

class ChromaKeyShowMask : public BC_CheckBox
{
public:
	ChromaKeyShowMask(ChromaKeyHSV *plugin, int x, int y);
	int handle_event();
	ChromaKeyHSV *plugin;
};



class ChromaKeyWindow : public PluginClientWindow
{
public:
	ChromaKeyWindow(ChromaKeyHSV *plugin);
	~ChromaKeyWindow();

	void create_objects();
	void update_sample();
	void update_gui(int clear);
	void done_event(int result);

	ChromaKeyColor *color;
	ChromaKeyUseColorPicker *use_colorpicker;

	ChromaKeyFText *min_brightness_text;
	ChromaKeyFSlider *min_brightness_slider;
	ChromaKeyClr *min_brightness_Clr;

	ChromaKeyFText *max_brightness_text;
	ChromaKeyFSlider *max_brightness_slider;
	ChromaKeyClr *max_brightness_Clr;

	ChromaKeyFText *saturation_text;
	ChromaKeyFSlider *saturation_slider;
	ChromaKeyClr *saturation_Clr;

	ChromaKeyFText *min_saturation_text;
	ChromaKeyFSlider *min_saturation_slider;
	ChromaKeyClr *min_saturation_Clr;

	ChromaKeyFText *tolerance_text;
	ChromaKeyFSlider *tolerance_slider;
	ChromaKeyClr *tolerance_Clr;

	ChromaKeyFText *in_slope_text;
	ChromaKeyFSlider *in_slope_slider;
	ChromaKeyClr *in_slope_Clr;

	ChromaKeyFText *out_slope_text;
	ChromaKeyFSlider *out_slope_slider;
	ChromaKeyClr *out_slope_Clr;

	ChromaKeyFText *alpha_offset_text;
	ChromaKeyFSlider *alpha_offset_slider;
	ChromaKeyClr *alpha_offset_Clr;

	ChromaKeyFText *spill_threshold_text;
	ChromaKeyFSlider *spill_threshold_slider;
	ChromaKeyClr *spill_threshold_Clr;

	ChromaKeyFText *spill_amount_text;
	ChromaKeyFSlider *spill_amount_slider;
	ChromaKeyClr *spill_amount_Clr;

	ChromaKeyShowMask *show_mask;
	ChromaKeyReset *reset;
	BC_SubWindow *sample;
	ChromaKeyHSV *plugin;
	ChromaKeyColorThread *color_thread;
};







class ChromaKeyServer : public LoadServer
{
public:
	ChromaKeyServer(ChromaKeyHSV *plugin);
	void init_packages();
	LoadClient* new_client();
	LoadPackage* new_package();
	ChromaKeyHSV *plugin;
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
	ChromaKeyUnit(ChromaKeyHSV *plugin, ChromaKeyServer *server);
	void process_package(LoadPackage *package);
	template <typename component_type> void process_chromakey(int components, component_type max, bool use_yuv, ChromaKeyPackage *pkg);
	bool is_same_color(float r, float g, float b, float rk,float bk,float gk, float color_threshold, float light_threshold, int key_main_component);

	ChromaKeyHSV *plugin;

};




class ChromaKeyHSV : public PluginVClient
{
public:
	ChromaKeyHSV(PluginServer *server);
	~ChromaKeyHSV();

	PLUGIN_CLASS_MEMBERS(ChromaKeyConfig);
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
