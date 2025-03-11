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

#ifndef BLENDALGEBRA_H
#define BLENDALGEBRA_H

#include "guicast.h"
#include "loadbalance.h"
#include "colorpicker.h"
#include "pluginvclient.h"

// Several forward declarations

class BlendAlgebra;
class BlendAlgebraConfig;
class BlendAlgebraWindow;
class BlendAlgebraAlphaText;
class BlendAlgebraAlphaSlider;
class BlendAlgebraFileBox;

// Plugin configuration class definition

class BlendAlgebraConfig
{
public:
  BlendAlgebraConfig();

  int equivalent(BlendAlgebraConfig &that);
  void copy_from(BlendAlgebraConfig &that);
  void interpolate(BlendAlgebraConfig &prev,
		   BlendAlgebraConfig &next,
		   int64_t prev_frame,
		   int64_t next_frame,
		   int64_t current_frame);

  int get_key_color();

  char funcname[BCTEXTLEN];
  int parallel;
  int clipcolors;
  int clear_input;

  static const char *direction_to_text(int direction);
  int direction;
  enum
  {
    BOTTOM_FIRST,
    TOP_FIRST
  };

  static const char *output_to_text(int output_track);
  int output_track;
  enum
  {
    TOP,
    BOTTOM
  };

  static const char *colorspace_to_text(int colorspace);
  int colorspace;
  enum
  {
    AUTO,				// requested from function
    RGB,
    YUV,
    HSV,
    PROJECT				// as defined in project settings
  };

  float red;				// key color to substitute for NaN
  float green;
  float blue;
  float alpha;				// alpha to substitute for NaN
};

// Plugin dialog window class definition

class BlendAlgebraFuncname : public BC_TextBox
{
public:
  BlendAlgebraFuncname(BlendAlgebra *plugin, const char *funcname,
		       BlendAlgebraWindow *gui, int x, int y);
  int handle_event();
  BlendAlgebra *plugin;
  BlendAlgebraWindow *gui;
};

class BlendAlgebraDetach : public BC_GenericButton
{
public:
  BlendAlgebraDetach(BlendAlgebra *plugin, BlendAlgebraWindow *gui,
		     int x, int y);
  int handle_event();
  BlendAlgebra *plugin;
  BlendAlgebraWindow *gui;
};

class BlendAlgebraEdit : public BC_GenericButton
{
public:
  BlendAlgebraEdit(BlendAlgebra *plugin, BlendAlgebraWindow *gui,
		   int x, int y);
  int handle_event();
  BlendAlgebra *plugin;
  BlendAlgebraWindow *gui;
};

class BlendAlgebraRefresh : public BC_GenericButton
{
public:
  BlendAlgebraRefresh(BlendAlgebra *plugin, BlendAlgebraWindow *gui,
		      int x, int y);
  int handle_event();
  BlendAlgebra *plugin;
  BlendAlgebraWindow *gui;
};

class BlendAlgebraToCurdir : public BC_GenericButton
{
public:
  BlendAlgebraToCurdir(BlendAlgebraFileBox *file_box, int x, int y);
  int handle_event();
  BlendAlgebraFileBox *file_box;
};

class BlendAlgebraToUsrlib : public BC_GenericButton
{
public:
  BlendAlgebraToUsrlib(BlendAlgebraFileBox *file_box, int x, int y);
  int handle_event();
  BlendAlgebraFileBox *file_box;
};

class BlendAlgebraToSyslib : public BC_GenericButton
{
public:
  BlendAlgebraToSyslib(BlendAlgebraFileBox *file_box, int x, int y);
  int handle_event();
  BlendAlgebraFileBox *file_box;
};

class BlendAlgebraCopyCurdir : public BC_GenericButton
{
public:
  BlendAlgebraCopyCurdir(BlendAlgebraFileBox *file_box, int x, int y);
  int handle_event();
  BlendAlgebraFileBox *file_box;
};

class BlendAlgebraCopyUsrlib : public BC_GenericButton
{
public:
  BlendAlgebraCopyUsrlib(BlendAlgebraFileBox *file_box, int x, int y);
  int handle_event();
  BlendAlgebraFileBox *file_box;
};

class BlendAlgebraFileEdit : public BC_GenericButton
{
public:
  BlendAlgebraFileEdit(BlendAlgebraFileBox *file_box, int x, int y);
  int handle_event();
  BlendAlgebraFileBox *file_box;
};

class BlendAlgebraFileBox : public BC_FileBox
{
public:
  BlendAlgebraFileBox(BlendAlgebra *plugin, BlendAlgebraWindow *gui,
		      char *init_path);
  ~BlendAlgebraFileBox();
  void add_objects();
  int resize_event(int w, int h);

  BlendAlgebra *plugin;
  BlendAlgebraWindow *gui;
  BlendAlgebraToCurdir *to_curdir;
  BlendAlgebraToUsrlib *to_usrlib;
  BlendAlgebraToSyslib *to_syslib;
  BlendAlgebraCopyCurdir *copy_curdir;
  BlendAlgebraCopyUsrlib *copy_usrlib;
  BlendAlgebraFileEdit *file_edit;

  int reinit_path;
  char changed_path[BCTEXTLEN];
};

class BlendAlgebraFileButton : public BC_GenericButton, public Thread
{
public:
  BlendAlgebraFileButton(BlendAlgebra *plugin, BlendAlgebraWindow *gui,
			 int x, int y);
  ~BlendAlgebraFileButton();
  int handle_event();
  void run();
  void stop();
  BlendAlgebra *plugin;
  BlendAlgebraWindow *gui;
  BlendAlgebraFileBox *file_box;
};

class BlendAlgebraDirection : public BC_PopupMenu
{
public:
  BlendAlgebraDirection(BlendAlgebra *plugin, int x, int y);
  void create_objects();
  int handle_event();
  BlendAlgebra *plugin;
};

class BlendAlgebraOutput : public BC_PopupMenu
{
public:
  BlendAlgebraOutput(BlendAlgebra *plugin, int x, int y);
  void create_objects();
  int handle_event();
  BlendAlgebra *plugin;
};

class BlendAlgebraColorspace : public BC_PopupMenu
{
public:
  BlendAlgebraColorspace(BlendAlgebra *plugin, int x, int y);
  void create_objects();
  int handle_event();
  BlendAlgebra *plugin;
};

class BlendAlgebraClipcolors : public BC_CheckBox
{
public:
  BlendAlgebraClipcolors(BlendAlgebra *plugin, BlendAlgebraWindow *gui,
			 int x, int y);
  int handle_event();
  BlendAlgebra *plugin;
  BlendAlgebraWindow *gui;
};

class BlendAlgebraParallel : public BC_CheckBox
{
public:
  BlendAlgebraParallel(BlendAlgebra *plugin, BlendAlgebraWindow *gui,
		       int x, int y);
  int handle_event();
  BlendAlgebra *plugin;
  BlendAlgebraWindow *gui;
};

class BlendAlgebraClearInput : public BC_CheckBox
{
public:
  BlendAlgebraClearInput(BlendAlgebra *plugin, BlendAlgebraWindow *gui,
			 int x, int y);
  int handle_event();
  BlendAlgebra *plugin;
  BlendAlgebraWindow *gui;
};

class BlendAlgebraKeyColor : public BC_GenericButton
{
public:
  BlendAlgebraKeyColor(BlendAlgebra *plugin, BlendAlgebraWindow *gui,
		       int x, int y);
  int handle_event();
  BlendAlgebra *plugin;
  BlendAlgebraWindow *gui;
};

class BlendAlgebraColorPicker : public BC_GenericButton
{
public:
  BlendAlgebraColorPicker(BlendAlgebra *plugin, BlendAlgebraWindow *gui,
			  int x, int y);
  int handle_event();
  BlendAlgebra *plugin;
  BlendAlgebraWindow *gui;
};

class BlendAlgebraColorThread : public ColorPicker
{
public:
  BlendAlgebraColorThread(BlendAlgebra *plugin, BlendAlgebraWindow *gui);
  int handle_new_color(int output, int alpha);
  BlendAlgebra *plugin;
  BlendAlgebraWindow *gui;
};

class BlendAlgebraAlphaText : public BC_TumbleTextBox
{
public:
  BlendAlgebraAlphaText(BlendAlgebra *plugin, BlendAlgebraWindow *gui,
			BlendAlgebraAlphaSlider *slider, int x, int y,
			float min, float max, float *output);
  ~BlendAlgebraAlphaText();
  int handle_event();
  BlendAlgebra *plugin;
  BlendAlgebraWindow *gui;
  BlendAlgebraAlphaSlider *slider;
  float *output;
  float min, max;
};

class BlendAlgebraAlphaSlider : public BC_FSlider
{
public:
  BlendAlgebraAlphaSlider(BlendAlgebra *plugin, BlendAlgebraAlphaText *text,
			  int x, int y, int w, float min, float max,
			  float *output);
  ~BlendAlgebraAlphaSlider();
  int handle_event();
  BlendAlgebra *plugin;
  BlendAlgebraAlphaText *text;
  float *output;
};

class BlendAlgebraWindow : public PluginClientWindow
{
public:
  BlendAlgebraWindow(BlendAlgebra *plugin);
  ~BlendAlgebraWindow();

  void create_objects();
  void update_key_sample();
  void done_event();
  int close_event();
  int hide_window(int flush=1);

  BlendAlgebra *plugin;
  BlendAlgebraFuncname *funcname;
  BlendAlgebraDirection *direction;
  BlendAlgebraOutput *output;
  BlendAlgebraColorspace *colorspace;
  BlendAlgebraClipcolors *clipcolors;
  BlendAlgebraParallel *parallel;
  BlendAlgebraClearInput *clear_input;
  BC_SubWindow *key_sample;
  BlendAlgebraKeyColor *key_color;
  BlendAlgebraColorPicker *color_picker;
  BlendAlgebraColorThread *color_thread;
  BlendAlgebraAlphaText *alpha_text;
  BlendAlgebraAlphaSlider *key_alpha;
  BlendAlgebraFileButton *file_button;
  BlendAlgebraDetach *detach_button;
  BlendAlgebraEdit *edit_button;
  BlendAlgebraRefresh *refresh_button;

  Mutex *editing_lock;
  int editing;
  int quit_now;
};

// For multithreading processing engine

class BlendAlgebraEngine : public LoadServer
{
public:
  BlendAlgebraEngine(BlendAlgebra *plugin,
		     int total_clients, int total_packages);
  void init_packages();
  LoadClient* new_client();
  LoadPackage* new_package();

  BlendAlgebra *plugin;
};

class BlendAlgebraPackage : public LoadPackage
{
public:
  BlendAlgebraPackage();

  int y1, y2;
};

class BlendAlgebraUnit : public LoadClient
{
public:
  BlendAlgebraUnit(BlendAlgebra *plugin, BlendAlgebraEngine *engine);
  void process_package(LoadPackage *package);

  BlendAlgebra *plugin;
  BlendAlgebraEngine *engine;
};

// Plugin main class definition

// User function prototypes, C style, processing and init entries
typedef void (*BAF_proc) (int,
			  float *, float *, float *, float *,
			  float,   float,   float,   float,
			  float *, float *, float *, float *,
			  int,     int,     int,     int, int);
typedef void (*BAF_init) (int *, int, int *, int, int *, int, int, int, int);

class BlendAlgebraFunc
{
public:
  BlendAlgebraFunc();
  ~BlendAlgebraFunc();

  char src[BCTEXTLEN];				// source filename
  void *handle;					// handle returned from dlopen()
  BAF_proc proc;		 // main processing entry from dlsym()
  BAF_init init;		// optional initializing entry from dlsym()
  time_t tstamp;		// timestamp when function was last linked
};

class BlendAlgebra : public PluginVClient
{
public:
  BlendAlgebra(PluginServer *server);
  ~BlendAlgebra();

  PLUGIN_CLASS_MEMBERS(BlendAlgebraConfig);

  int process_buffer(VFrame **frame, int64_t start_position, double frame_rate);
  int is_realtime();
  int is_multichannel();
  int is_synthesis();
  void save_data(KeyFrame *keyframe);
  void read_data(KeyFrame *keyframe);
  void update_gui();
  void process_frames(int y1, int y2);

  // this flag set in constructor, cleared after first load_configuration()
  int inspect_configuration;

  int layers;				// no of tracks
  int width, height;			// frame dimensions
  int color_proj;			// project color model
  int color_work;			// working color space in the function
  int has_alpha;			// 1 == has alpha channel

  // color components in three color spaces, used to substitute
  // for NaN or infinity in invalid results, or as a chroma key
  float rgb_r, rgb_g, rgb_b, yuv_y, yuv_u, yuv_v, hsv_h, hsv_s, hsv_v, key_a;

  BlendAlgebraFunc curr_func;			// currently active entry point
  int curr_func_no;				// no of current entry in list
  ArrayList<BlendAlgebraFunc*> funclist;	// list of known entry points

  VFrame **frame;				// pointer to frames to process

  Mutex *func_lock;

  BlendAlgebraEngine *engine;			// for parallelized processing
};

#endif	/* BLENDALGEBRA_H */
