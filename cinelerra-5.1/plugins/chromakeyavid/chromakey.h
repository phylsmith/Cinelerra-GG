
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
#define RESET_SATURATION_START  5
#define RESET_SATURATION_LINE   6
#define RESET_IN_SLOPE          7
#define RESET_OUT_SLOPE         8
#define RESET_ALPHA_OFFSET      9
#define RESET_SPILL_SATURATION 10
#define RESET_SPILL_ANGLE      11

class ChromaKeyAvid;
class ChromaKeyWindow;
class ChromaKeyText;
class ChromaKeySlider;
class ChromaKeyClr;
class ChromaKeyReset;
class ChromaKeyStore;
class ChromaKeyRecall;
class ChromaKeyExchange;
class ChromaKeyUndo;

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
  // distance of wedge point from the center
  float saturation_start;
  // minimum saturation after wedge point
  float saturation_line;
  // hue range
  float tolerance;
  // Mask feathering
  float in_slope;
  float out_slope;
  float alpha_offset;
  // Spill light compensation
  float spill_saturation;
  float spill_angle;
  bool  desaturate_only;
  // Instances for Store
  bool  store_show_mask;
  float store_red;
  float store_green;
  float store_blue;
  float store_min_brightness;
  float store_max_brightness;
  float store_saturation_start;
  float store_saturation_line;
  float store_tolerance;
  float store_in_slope;
  float store_out_slope;
  float store_alpha_offset;
  float store_spill_saturation;
  float store_spill_angle;
  bool  store_desaturate_only;
  // Instances for Undo
  bool  undo_show_mask;
  float undo_red;
  float undo_green;
  float undo_blue;
  float undo_min_brightness;
  float undo_max_brightness;
  float undo_saturation_start;
  float undo_saturation_line;
  float undo_tolerance;
  float undo_in_slope;
  float undo_out_slope;
  float undo_alpha_offset;
  float undo_spill_saturation;
  float undo_spill_angle;
  bool  undo_desaturate_only;
};


class ChromaKeyColor : public BC_GenericButton
{
public:
  ChromaKeyColor(ChromaKeyAvid *plugin,
		 ChromaKeyWindow *gui,
		 int x,
		 int y);

  int handle_event();

  ChromaKeyAvid *plugin;
  ChromaKeyWindow *gui;
};

class ChromaKeyReset : public BC_GenericButton
{
public:
  ChromaKeyReset(ChromaKeyAvid *plugin, ChromaKeyWindow *gui, int x, int y);
  int handle_event();
  ChromaKeyAvid *plugin;
  ChromaKeyWindow *gui;
};

class ChromaKeyStore : public BC_GenericButton
{
public:
  ChromaKeyStore(ChromaKeyAvid *plugin, ChromaKeyWindow *gui, int x, int y);
  int handle_event();
  ChromaKeyAvid *plugin;
  ChromaKeyWindow *gui;
};

class ChromaKeyRecall : public BC_GenericButton
{
public:
  ChromaKeyRecall(ChromaKeyAvid *plugin, ChromaKeyWindow *gui, int x, int y);
  int handle_event();
  ChromaKeyAvid *plugin;
  ChromaKeyWindow *gui;
};

class ChromaKeyExchange : public BC_GenericButton
{
public:
  ChromaKeyExchange(ChromaKeyAvid *plugin, ChromaKeyWindow *gui, int x, int y);
  int handle_event();
  ChromaKeyAvid *plugin;
  ChromaKeyWindow *gui;
};

class ChromaKeyUndo : public BC_GenericButton
{
public:
  ChromaKeyUndo(ChromaKeyAvid *plugin, ChromaKeyWindow *gui, int x, int y);
  int handle_event();
  ChromaKeyAvid *plugin;
  ChromaKeyWindow *gui;
};

class ChromaKeyText : public BC_TumbleTextBox
{
public:
  ChromaKeyText(ChromaKeyAvid *plugin, ChromaKeyWindow *gui,
		ChromaKeySlider *slider, int x, int y,
		float min, float max, float *output);
  ~ChromaKeyText();
  int handle_event();
  ChromaKeyAvid *plugin;
  ChromaKeyWindow *gui;
  ChromaKeySlider *slider;
  float *output;
  float min, max;
};

class ChromaKeySlider : public BC_FSlider
{
public:
  ChromaKeySlider(ChromaKeyAvid *plugin, ChromaKeyText *text,
		  int x, int y, int w, float min, float max, float *output);
  ~ChromaKeySlider();
  int handle_event();
  ChromaKeyAvid *plugin;
  ChromaKeyText *text;
  float *output;
};

class ChromaKeyClr : public BC_Button
{
public:
  ChromaKeyClr(ChromaKeyAvid *plugin, ChromaKeyWindow *gui,
	       int x, int y, int clear);
  ~ChromaKeyClr();
  int handle_event();
  ChromaKeyAvid *plugin;
  ChromaKeyWindow *gui;
  int clear;
};


class ChromaKeyUseColorPicker : public BC_GenericButton
{
public:
  ChromaKeyUseColorPicker(ChromaKeyAvid *plugin,
			  ChromaKeyWindow *gui, int x, int y);
  int handle_event();
  ChromaKeyAvid *plugin;
  ChromaKeyWindow *gui;
};

class ChromaKeyColorThread : public ColorPicker
{
public:
  ChromaKeyColorThread(ChromaKeyAvid *plugin, ChromaKeyWindow *gui);
  int handle_new_color(int output, int alpha);
  ChromaKeyAvid *plugin;
  ChromaKeyWindow *gui;
};

class ChromaKeyToggle : public BC_CheckBox
{
public:
  ChromaKeyToggle(ChromaKeyAvid *plugin, 
		  int x, 
		  int y,
		  bool *output,
		  const char *caption);
  int handle_event();
  ChromaKeyAvid *plugin;
  bool *output;
};



class ChromaKeyWindow : public PluginClientWindow
{
public:
  ChromaKeyWindow(ChromaKeyAvid *plugin);
  ~ChromaKeyWindow();

  void create_objects();
  void update_sample();
  void update_gui(int clear);
  void done_event(int result);

  ChromaKeyColor *color;
  ChromaKeyUseColorPicker *use_colorpicker;

  ChromaKeyText   *min_brightness_text;
  ChromaKeySlider *min_brightness;
  ChromaKeyClr    *min_brightness_clr;

  ChromaKeyText   *max_brightness_text;
  ChromaKeySlider *max_brightness;
  ChromaKeyClr    *max_brightness_clr;

  ChromaKeyText   *saturation_start_text;
  ChromaKeySlider *saturation_start;
  ChromaKeyClr    *saturation_start_clr;

  ChromaKeyText   *saturation_line_text;
  ChromaKeySlider *saturation_line;
  ChromaKeyClr    *saturation_line_clr;

  ChromaKeyText   *tolerance_text;
  ChromaKeySlider *tolerance;
  ChromaKeyClr    *tolerance_clr;

  ChromaKeyText   *in_slope_text;
  ChromaKeySlider *in_slope;
  ChromaKeyClr    *in_slope_clr;

  ChromaKeyText   *out_slope_text;
  ChromaKeySlider *out_slope;
  ChromaKeyClr    *out_slope_clr;

  ChromaKeyText   *alpha_offset_text;
  ChromaKeySlider *alpha_offset;
  ChromaKeyClr    *alpha_offset_clr;

  ChromaKeyText   *spill_saturation_text;
  ChromaKeySlider *spill_saturation;
  ChromaKeyClr    *spill_saturation_clr;

  ChromaKeyText   *spill_angle_text;
  ChromaKeySlider *spill_angle;
  ChromaKeyClr    *spill_angle_clr;

  ChromaKeyToggle *desaturate_only;
  ChromaKeyToggle *show_mask;

  ChromaKeyReset    *reset;
  ChromaKeyStore    *store;
  ChromaKeyRecall   *recall;
  ChromaKeyExchange *exchange;
  ChromaKeyUndo     *undo;

  BC_SubWindow *sample;
  ChromaKeyAvid *plugin;
  ChromaKeyColorThread *color_thread;
};

class ChromaKeyServer : public LoadServer
{
public:
  ChromaKeyServer(ChromaKeyAvid *plugin);
  void init_packages();
  LoadClient* new_client();
  LoadPackage* new_package();

  ChromaKeyAvid *plugin;
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
  ChromaKeyUnit(ChromaKeyAvid *plugin, ChromaKeyServer *server);
  void process_package(LoadPackage *package);
  template <typename component_type> void process_chromakey(int components, component_type max, bool use_yuv, ChromaKeyPackage *pkg);
  bool is_same_color(float r, float g, float b, float rk,float bk,float gk,
		     float color_threshold, float light_threshold,
		     int key_main_component);

  ChromaKeyAvid *plugin;
};

class ChromaKeyAvid : public PluginVClient
{
public:
  ChromaKeyAvid(PluginServer *server);
  ~ChromaKeyAvid();

  PLUGIN_CLASS_MEMBERS(ChromaKeyConfig);
  int process_buffer(VFrame *frame, int64_t start_position, double frame_rate);
  int handle_opengl();
  int is_realtime();
  void save_data(KeyFrame *keyframe);
  void read_data(KeyFrame *keyframe);
  void update_gui();

  VFrame *input, *output;
  ChromaKeyServer *engine;
};

#endif
