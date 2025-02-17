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

#ifndef BLENDPROGRAM_H
#define BLENDPROGRAM_H

#include "guicast.h"
#include "loadbalance.h"
#include "colorpicker.h"
#include "pluginvclient.h"

// Several forward declarations

class BlendProgram;
class BlendProgramConfig;
class BlendProgramWindow;
class BlendProgramAlphaText;
class BlendProgramAlphaSlider;
class BlendProgramFileBox;

// Plugin configuration class definition

class BlendProgramConfig
{
public:
  BlendProgramConfig();

  int equivalent(BlendProgramConfig &that);
  void copy_from(BlendProgramConfig &that);
  void interpolate(BlendProgramConfig &prev,
		   BlendProgramConfig &next,
		   int64_t prev_frame,
		   int64_t next_frame,
		   int64_t current_frame);

  int get_key_color();

  char funcname[BCTEXTLEN];
  int parallel;
  int clipcolors;

  static const char *direction_to_text(int direction);
  int direction;
  enum
  {
    BOTTOM_FIRST,
    TOP_FIRST
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

class BlendProgramFuncname : public BC_TextBox
{
public:
  BlendProgramFuncname(BlendProgram *plugin, const char *funcname,
		       BlendProgramWindow *gui, int x, int y);
  int handle_event();
  BlendProgram *plugin;
  BlendProgramWindow *gui;
};

class BlendProgramDetach : public BC_GenericButton
{
public:
  BlendProgramDetach(BlendProgram *plugin, BlendProgramWindow *gui,
		     int x, int y);
  int handle_event();
  BlendProgram *plugin;
  BlendProgramWindow *gui;
};

class BlendProgramEdit : public BC_GenericButton
{
public:
  BlendProgramEdit(BlendProgram *plugin, BlendProgramWindow *gui,
		   int x, int y);
  int handle_event();
  BlendProgram *plugin;
  BlendProgramWindow *gui;
};

class BlendProgramRefresh : public BC_GenericButton
{
public:
  BlendProgramRefresh(BlendProgram *plugin, BlendProgramWindow *gui,
		      int x, int y);
  int handle_event();
  BlendProgram *plugin;
  BlendProgramWindow *gui;
};

class BlendProgramToCurdir : public BC_GenericButton
{
public:
  BlendProgramToCurdir(BlendProgramFileBox *file_box, int x, int y);
  int handle_event();
  BlendProgramFileBox *file_box;
};

class BlendProgramToUsrlib : public BC_GenericButton
{
public:
  BlendProgramToUsrlib(BlendProgramFileBox *file_box, int x, int y);
  int handle_event();
  BlendProgramFileBox *file_box;
};

class BlendProgramToSyslib : public BC_GenericButton
{
public:
  BlendProgramToSyslib(BlendProgramFileBox *file_box, int x, int y);
  int handle_event();
  BlendProgramFileBox *file_box;
};

class BlendProgramCopyCurdir : public BC_GenericButton
{
public:
  BlendProgramCopyCurdir(BlendProgramFileBox *file_box, int x, int y);
  int handle_event();
  BlendProgramFileBox *file_box;
};

class BlendProgramCopyUsrlib : public BC_GenericButton
{
public:
  BlendProgramCopyUsrlib(BlendProgramFileBox *file_box, int x, int y);
  int handle_event();
  BlendProgramFileBox *file_box;
};

class BlendProgramFileEdit : public BC_GenericButton
{
public:
  BlendProgramFileEdit(BlendProgramFileBox *file_box, int x, int y);
  int handle_event();
  BlendProgramFileBox *file_box;
};

class BlendProgramFileBox : public BC_FileBox
{
public:
  BlendProgramFileBox(BlendProgram *plugin, BlendProgramWindow *gui,
		      char *init_path);
  ~BlendProgramFileBox();
  void add_objects();
  int resize_event(int w, int h);

  BlendProgram *plugin;
  BlendProgramWindow *gui;
  BlendProgramToCurdir *to_curdir;
  BlendProgramToUsrlib *to_usrlib;
  BlendProgramToSyslib *to_syslib;
  BlendProgramCopyCurdir *copy_curdir;
  BlendProgramCopyUsrlib *copy_usrlib;
  BlendProgramFileEdit *file_edit;

  int reinit_path;
};

class BlendProgramFileButton : public BC_GenericButton, public Thread
{
public:
  BlendProgramFileButton(BlendProgram *plugin, BlendProgramWindow *gui,
			 int x, int y);
  ~BlendProgramFileButton();
  int handle_event();
  void run();
  void stop();
  BlendProgram *plugin;
  BlendProgramWindow *gui;
  BlendProgramFileBox *file_box;
};

class BlendProgramDirection : public BC_PopupMenu
{
public:
  BlendProgramDirection(BlendProgram *plugin, int x, int y);
  void create_objects();
  int handle_event();
  BlendProgram *plugin;
};

class BlendProgramColorspace : public BC_PopupMenu
{
public:
  BlendProgramColorspace(BlendProgram *plugin, int x, int y);
  void create_objects();
  int handle_event();
  BlendProgram *plugin;
};

class BlendProgramClipcolors : public BC_CheckBox
{
public:
  BlendProgramClipcolors(BlendProgram *plugin, BlendProgramWindow *gui,
			 int x, int y);
  int handle_event();
  BlendProgram *plugin;
  BlendProgramWindow *gui;
};

class BlendProgramParallel : public BC_CheckBox
{
public:
  BlendProgramParallel(BlendProgram *plugin, BlendProgramWindow *gui,
		       int x, int y);
  int handle_event();
  BlendProgram *plugin;
  BlendProgramWindow *gui;
};

class BlendProgramKeyColor : public BC_GenericButton
{
public:
  BlendProgramKeyColor(BlendProgram *plugin, BlendProgramWindow *gui,
		       int x, int y);
  int handle_event();
  BlendProgram *plugin;
  BlendProgramWindow *gui;
};

class BlendProgramColorPicker : public BC_GenericButton
{
public:
  BlendProgramColorPicker(BlendProgram *plugin, BlendProgramWindow *gui,
			  int x, int y);
  int handle_event();
  BlendProgram *plugin;
  BlendProgramWindow *gui;
};

class BlendProgramColorThread : public ColorPicker
{
public:
  BlendProgramColorThread(BlendProgram *plugin, BlendProgramWindow *gui);
  int handle_new_color(int output, int alpha);
  BlendProgram *plugin;
  BlendProgramWindow *gui;
};

class BlendProgramAlphaText : public BC_TumbleTextBox
{
public:
  BlendProgramAlphaText(BlendProgram *plugin, BlendProgramWindow *gui,
			BlendProgramAlphaSlider *slider, int x, int y,
			float min, float max, float *output);
  ~BlendProgramAlphaText();
  int handle_event();
  BlendProgram *plugin;
  BlendProgramWindow *gui;
  BlendProgramAlphaSlider *slider;
  float *output;
  float min, max;
};

class BlendProgramAlphaSlider : public BC_FSlider
{
public:
  BlendProgramAlphaSlider(BlendProgram *plugin, BlendProgramAlphaText *text,
			  int x, int y, int w, float min, float max,
			  float *output);
  ~BlendProgramAlphaSlider();
  int handle_event();
  BlendProgram *plugin;
  BlendProgramAlphaText *text;
  float *output;
};

class BlendProgramWindow : public PluginClientWindow
{
public:
  BlendProgramWindow(BlendProgram *plugin);
  ~BlendProgramWindow();

  void create_objects();
  void update_key_sample();
  void done_event();
  int close_event();
  int hide_window(int flush=1);

  BlendProgram *plugin;
  BlendProgramFuncname *funcname;
  BlendProgramDirection *direction;
  BlendProgramColorspace *colorspace;
  BlendProgramClipcolors *clipcolors;
  BlendProgramParallel *parallel;
  BC_SubWindow *key_sample;
  BlendProgramKeyColor *key_color;
  BlendProgramColorPicker *color_picker;
  BlendProgramColorThread *color_thread;
  BlendProgramAlphaText *alpha_text;
  BlendProgramAlphaSlider *key_alpha;
  BlendProgramFileButton *file_button;
  BlendProgramDetach *detach_button;
  BlendProgramEdit *edit_button;
  BlendProgramRefresh *refresh_button;

  Mutex *editing_lock;
  int editing;
};

// For multithreading processing engine

class BlendProgramEngine : public LoadServer
{
public:
  BlendProgramEngine(BlendProgram *plugin,
		     int total_clients, int total_packages);
  void init_packages();
  LoadClient* new_client();
  LoadPackage* new_package();

  BlendProgram *plugin;
};

class BlendProgramPackage : public LoadPackage
{
public:
  BlendProgramPackage();

  int y1, y2;
};

class BlendProgramUnit : public LoadClient
{
public:
  BlendProgramUnit(BlendProgram *plugin, BlendProgramEngine *engine);
  void process_package(LoadPackage *package);

  BlendProgram *plugin;
  BlendProgramEngine *engine;
};

// Plugin main class definition

// User function prototypes, C style, processing and init entries
typedef void (*BPF_proc) (int, float *, float *, float *, float *,
			  float, float, float, float, int, int, int, int, int);
typedef void (*BPF_init) (int *, int, int *, int, int *, int, int, int, int);

class BlendProgramFunc
{
public:
  BlendProgramFunc();
  ~BlendProgramFunc();

  char src[BCTEXTLEN];				// source filename
  void *handle;					// handle returned from dlopen()
  BPF_proc proc;		 // main processing entry from dlsym()
  BPF_init init;		// optional initializing entry from dlsym()
  time_t tstamp;		// timestamp when function was last linked
};

class BlendProgram : public PluginVClient
{
public:
  BlendProgram(PluginServer *server);
  ~BlendProgram();

  PLUGIN_CLASS_MEMBERS(BlendProgramConfig);

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

  BlendProgramFunc curr_func;			// currently active entry point
  int curr_func_no;				// no of current entry in list
  ArrayList<BlendProgramFunc*> funclist;	// list of known entry points

  VFrame **frame;				// pointer to frames to process

  Mutex *func_lock;

  BlendProgramEngine *engine;			// for parallelized processing
};

#endif	/* BLENDPROGRAM_H */
