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

#include "filexml.h"
#include "keyframe.h"
#include "language.h"
#include "mainerror.h"
#include "clip.h"
#include "bccolors.h"
#include "loadbalance.h"
#include "filesystem.h"
#include "vframe.h"
#include "mainsession.h"
#include "mwindow.h"
#include "pluginserver.h"
#include "blendalgebra.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dlfcn.h>

//#define DEBUG

// Sorry for this global variable, it is needed to propagate the signal
// from the GUI instance to the processing instance of the plugin
// that some user function might have changed and need recompilation
static time_t BlendAlgebraTstamp = -1;

REGISTER_PLUGIN(BlendAlgebra)

LOAD_CONFIGURATION_MACRO(BlendAlgebra, BlendAlgebraConfig)

NEW_WINDOW_MACRO(BlendAlgebra, BlendAlgebraWindow)

const char *BlendAlgebra::plugin_title() { return N_("Blend Algebra"); }

int BlendAlgebra::is_realtime()     { return 1; }
int BlendAlgebra::is_multichannel() { return 1; }
int BlendAlgebra::is_synthesis()    { return 1; }

////////////////////////////////////////////
// Plugin configuration class implementation
////////////////////////////////////////////

BlendAlgebraConfig::BlendAlgebraConfig()
{
  funcname[0]  = 0;				// no function per default
  parallel     = 1;				// parallelize per default
  clipcolors   = 1;				// clip colors per default
  clear_input  = 1;				// like Overlay plugin does
  direction    = BlendAlgebraConfig::BOTTOM_FIRST; // as in Overlay plugin
  output_track = BlendAlgebraConfig::TOP;	// as in Overlay plugin
  colorspace   = BlendAlgebraConfig::AUTO;	// requested from function
  red = green = blue = 0;			// black key color per default
  alpha = 0;					// transparent per default
}

int BlendAlgebraConfig::equivalent(BlendAlgebraConfig &that)
{
  return
    !strcmp (funcname, that.funcname) &&
    parallel     == that.parallel     &&
    clipcolors   == that.clipcolors   &&
    clear_input  == that.clear_input  &&
    direction    == that.direction    &&
    output_track == that.output_track &&
    colorspace   == that.colorspace   &&
    EQUIV (red,   that.red)           &&
    EQUIV (green, that.green)         &&
    EQUIV (blue,  that.blue)          &&
    EQUIV (alpha, that.alpha);
}

void BlendAlgebraConfig::copy_from(BlendAlgebraConfig &that)
{
  strcpy (funcname, that.funcname);
  parallel     = that.parallel;
  clipcolors   = that.clipcolors;
  clear_input  = that.clear_input;
  direction    = that.direction;
  output_track = that.output_track;
  colorspace   = that.colorspace;
  red          = that.red;
  green        = that.green;
  blue         = that.blue;
  alpha        = that.alpha;
}

void BlendAlgebraConfig::interpolate (BlendAlgebraConfig &prev,
				      BlendAlgebraConfig &next,
				      int64_t prev_frame,
				      int64_t next_frame,
				      int64_t current_frame)
{
  double next_scale =
    (double) (current_frame - prev_frame) / (next_frame - prev_frame);
  double prev_scale =
    (double) (next_frame - current_frame) / (next_frame - prev_frame);

  red   = prev.red   * prev_scale + next.red   * next_scale;
  green = prev.green * prev_scale + next.green * next_scale;
  blue  = prev.blue  * prev_scale + next.blue  * next_scale;
  alpha = prev.alpha * prev_scale + next.alpha * next_scale;

  strcpy (funcname, prev.funcname);
  parallel     = prev.parallel;
  clipcolors   = prev.clipcolors;
  clear_input  = prev.clear_input;
  direction    = prev.direction;
  output_track = prev.output_track;
  colorspace   = prev.colorspace;
}

const char *BlendAlgebraConfig::direction_to_text(int direction)
{
  switch(direction)
  {
  case BlendAlgebraConfig::BOTTOM_FIRST: return _("Bottom first");
  case BlendAlgebraConfig::TOP_FIRST:    return _("Top first");
  }
  return "";
}

const char *BlendAlgebraConfig::output_to_text(int output_track)
{
  switch(output_track)
  {
  case BlendAlgebraConfig::TOP:    return _("Top");
  case BlendAlgebraConfig::BOTTOM: return _("Bottom");
  }
  return "";
}

const char *BlendAlgebraConfig::colorspace_to_text(int colorspace)
{
  switch(colorspace)
  {
  case BlendAlgebraConfig::AUTO:    return _("auto");
  case BlendAlgebraConfig::RGB:     return _("RGB");
  case BlendAlgebraConfig::YUV:     return _("YUV");
  case BlendAlgebraConfig::HSV:     return _("HSV");
  case BlendAlgebraConfig::PROJECT: return _("of project");
  }
  return "";
}

int BlendAlgebraConfig::get_key_color()
{
  int red   = (int) (CLIP (this->red,   0, 1) * 255);
  int green = (int) (CLIP (this->green, 0, 1) * 255);
  int blue  = (int) (CLIP (this->blue,  0, 1) * 255);
  return (red << 16) | (green << 8) | blue;
}

////////////////////////////////////////////
// Plugin dialog window class implementation
////////////////////////////////////////////

BlendAlgebraFuncname::BlendAlgebraFuncname(BlendAlgebra *plugin,
					   const char *funcname,
					   BlendAlgebraWindow *gui,
					   int x, int y)
  : BC_TextBox(x, y, gui->get_w()-x-xS(10), 1, funcname)
{
  this->plugin = plugin;
  this->gui = gui;
}

int BlendAlgebraFuncname::handle_event()
{
  // Perhaps locking is not needed here
  // as GUI is driven by a separate plugin instance
  plugin->func_lock->lock("BlendAlgebraFuncname::handle_event");
  strncpy(plugin->config.funcname, get_text(),
	  sizeof(plugin->config.funcname)-1);
  BlendAlgebraTstamp = time(NULL);	// time of possible function change
#ifdef DEBUG
  printf ("BlendAlgebraFuncname::handle_event setting function %s\n   timestamp %s",
	  plugin->config.funcname, ctime(&BlendAlgebraTstamp));
#endif
  plugin->func_lock->unlock();
  plugin->send_configure_change();
  return 1;
}

BlendAlgebraDetach::BlendAlgebraDetach (BlendAlgebra *plugin,
					BlendAlgebraWindow *gui,
					int x, int y)
  : BC_GenericButton (x, y, _("Detach"))
{
  this->plugin = plugin;
  this->gui = gui;
}

int BlendAlgebraDetach::handle_event()
{
  if (! plugin->config.funcname[0]) return 1;// already detached, nothing to do

  plugin->func_lock->lock("BlendAlgebraDetach::handle_event");
  plugin->config.funcname[0] = 0;	// clear function, inducing detach
  BlendAlgebraTstamp = time(NULL);	// force refresh of dlopen'd functions
#ifdef DEBUG
  printf ("BlendAlgebraDetach::handle_event clearing function\n   timestamp %s",
	  ctime(&BlendAlgebraTstamp));
#endif
  plugin->func_lock->unlock();

  gui->lock_window("BlendAlgebraDetach::handle_event");
  gui->funcname->update(plugin->config.funcname);
  gui->unlock_window();

  plugin->send_configure_change();
  return 1;
}

BlendAlgebraRefresh::BlendAlgebraRefresh (BlendAlgebra *plugin,
					  BlendAlgebraWindow *gui,
					  int x, int y)
  : BC_GenericButton (x, y, _("Refresh"))
{
  this->plugin = plugin;
  this->gui = gui;
}

int BlendAlgebraRefresh::handle_event()
{
  plugin->func_lock->lock("BlendAlgebraRefresh::handle_event");
  BlendAlgebraTstamp = time(NULL);	// force refresh of dlopen'd functions
#ifdef DEBUG
  printf ("BlendAlgebraRefresh::handle_event timestamp %s",
	  ctime(&BlendAlgebraTstamp));
#endif
  plugin->func_lock->unlock();
  plugin->send_configure_change();// no reconfigure, just recheck all functions
  return 1;
}

BlendAlgebraEdit::BlendAlgebraEdit (BlendAlgebra *plugin,
				    BlendAlgebraWindow *gui,
				    int x, int y)
  : BC_GenericButton (x, y, _("Edit..."))
{
  this->plugin = plugin;
  this->gui = gui;
}

int BlendAlgebraEdit::handle_event()
{
  char fname[BCTEXTLEN], dir[BCTEXTLEN], str[2*BCTEXTLEN];

  strcpy (fname, plugin->config.funcname);
  if (! fname[0])
  {
    eprintf (_("Blend Algebra: no source file to edit, select function first\n"));
    return 1;
  }

  // Evtl make function path absolute by prepending project path to it
  if (fname[0] != '/')	// fname is relative, prepend current project path
  {
    strcpy (dir, plugin->server->mwindow->session->filename);
    if (dir[0])
    {
      char *cp = strrchr (dir, '/');
      if (cp)
      {
	cp[1] = 0;		// strip project filename off from project path
	strcat (dir, fname);	// concatenate obtained path with function name
	strcpy (fname, dir);
      }
    }
  }

  // This will run configured external editor via perl script
  // If editor start is not backgrounded, GUI will block until editor exits
  sprintf(str, "\"%s/dlfcn/BlendAlgebraCompile.pl\" -edit \"%s\"",
	  getenv("CIN_DAT"), fname);
#ifdef DEBUG
  printf ("BlendAlgebraEdit::handle_event: executing:\n   %s\n", str);
#endif
  system (str);				// runs configured external editor

  plugin->func_lock->lock("BlendAlgebraEdit::handle_event");
  BlendAlgebraTstamp = time(NULL);	// force refresh of dlopen'd functions
#ifdef DEBUG
  printf ("BlendAlgebraEdit::handle_event edited function %s\n   timestamp %s",
	  fname, ctime(&BlendAlgebraTstamp));
#endif
  plugin->func_lock->unlock();

  // Evtl functions will be recompiled, but no configure change
  return 1;
}

BlendAlgebraFileButton::BlendAlgebraFileButton(BlendAlgebra *plugin,
					       BlendAlgebraWindow *gui,
					       int x, int y)
  : BC_GenericButton(x, y, _("Attach..."))
{
  this->plugin = plugin;
  this->gui = gui;
  this->file_box = 0;
}

BlendAlgebraFileButton::~BlendAlgebraFileButton()
{
  stop();
}

int BlendAlgebraFileButton::handle_event()
{
  gui->editing_lock->lock();

  if (! gui->editing)
  {
    gui->editing = 1;
    gui->editing_lock->unlock();
    start();
  }
  else
  {
    flicker();
    gui->editing_lock->unlock();
  }

  return 1;
}

void BlendAlgebraFileButton::run()
{
  int result = 1;
  char *cp;
  const char *fpath;
  char fname[BCTEXTLEN], dir[BCTEXTLEN];
  BC_Resources *resources;

  strcpy (fname, plugin->config.funcname);

  // This infinite loop is exited after clicking OK or Cancel in FileBox.
  // There are several special buttons which replace the FileBox initial path
  // with another predefined path and close FileBox with reinit_path flag set.
  // If reinit_path is set, the loop is repeated with that extracted path.
  // reinit_path is cleared inside BlendAlgebraFileBox constructor.

  for (;;)		// will exit when reinit_path == 0
  {	// Evtl make function path absolute by prepending project path to it
    if (fname[0] != '/') // fname is relative, prepend current project path
    {
      strcpy (dir, plugin->server->mwindow->session->filename);
      if (dir[0])
      {
	cp = strrchr (dir, '/');
	if (cp)
	{
	  cp[1] = 0;		// strip project filename off from project path
	  strcat (dir, fname);	// concatenate obtained path with function name
	  strcpy (fname, dir);
	}
      }
    }
#ifdef DEBUG
    printf ("BlendAlgebraFileButton::run creating file_box (%s)\n", fname);
#endif
    file_box = new BlendAlgebraFileBox (plugin, gui, fname);

    // Problem: if we call create_objects() right now, the most recently
    // visited path from history will be taken, not that where the current
    // function is. We have to update visited directory history beforehand,
    // but we cannot call update_history() before create_objects(),
    // ListBox recent_popup is not yet created, this can induce SEGV.
    // Therefore we must replicate updating history here explicitly.
    resources = get_resources();
    strcpy (dir, fname);
    cp = strrchr (dir, '/');
    if (cp)		// slash found
    {
      int oldest_id = 0x7fffffff, oldest = -1, match = -1, empty = -1;
      cp[1] = 0;
      for (int i=0; i<FILEBOX_HISTORY_SIZE; i++)	// scan all history
      {
	if (resources->filebox_history[i].path[0]) // history slot not empty
	{
	  if (! strcmp (resources->filebox_history[i].path, dir))
	  {
	    resources->filebox_history[i].id = resources->get_filebox_id();
	    match = i;
	    break;	// matched path already in history, nothing to do
	  }
	  if (resources->filebox_history[i].id < oldest_id)
	  {
	    oldest_id = resources->filebox_history[i].id;
	    oldest = i;	// memorize oldest slot
	  }
	}		// if history slot not empty
	else empty = i;	// empty history slot found
      }			// scan all history
      if (match < 0)	// matched path not found, insert one
      {
	if (empty < 0)	// no empty slot, free oldest one, create new entry
	{
	  for (int i=oldest; i<FILEBOX_HISTORY_SIZE-1; i++)
	  {
	    strcpy (resources->filebox_history[i].path,
		    resources->filebox_history[i+1].path);
	    resources->filebox_history[i].id =
	      resources->filebox_history[i+1].id;
	  }
	  empty = FILEBOX_HISTORY_SIZE-1;
	}		// if no empty slot
	strcpy (resources->filebox_history[empty].path, dir);
	resources->filebox_history[empty].id = resources->get_filebox_id();
	int done = 0;
	while (! done)	// alphabetize contents
	{
	  done = 1;
	  for (int i=1; i<FILEBOX_HISTORY_SIZE; i++)
	  {
	    if ((resources->filebox_history[i-1].path[0] &&
		 resources->filebox_history[i].path[0] &&
		 strcasecmp (resources->filebox_history[i-1].path,
			     resources->filebox_history[i].path) > 0) ||
		(resources->filebox_history[i-1].path[0] == 0 &&
		 resources->filebox_history[i].path[0]))
	    {
	      done = 0;
	      int id_temp;
	      strcpy (dir, resources->filebox_history[i-1].path);
	      id_temp = resources->filebox_history[i-1].id;
	      strcpy (resources->filebox_history[i-1].path,
		      resources->filebox_history[i].path);
	      resources->filebox_history[i-1].id =
		resources->filebox_history[i].id;
	      strcpy (resources->filebox_history[i].path, dir);
	      resources->filebox_history[i].id = id_temp;
	    }
	  }	// for FILEBOX_HISTORY_SIZE
	}	// while ! done
      }		// if matched path not found
    }		// if slash found

    // Visited directory history updated, we can create_objects() now.
    file_box->create_objects();
    file_box->lock_window ("BlendAlgebraFileButton::run");
    file_box->add_objects();			// add our special buttons
    file_box->update_filter ("*.ba");
    file_box->unlock_window();
    result = file_box->run_window();
    if (gui->quit_now)			// plugin dialog closed, imitate Cancel
    {
      delete file_box;
      file_box = 0;
      return;
    }
    if (file_box->reinit_path)			// if set, a button was clicked
    {
#ifdef DEBUG
      printf ("BlendAlgebraFileButton::run file_box returned %d reinit_path=%d\n   changed_path=%s\n",
	      result, file_box->reinit_path, file_box->changed_path);
#endif
      strcpy (fname, file_box->changed_path);	// this path was set by buttons
      delete file_box;
      file_box = 0;
      continue;	// reinit_path will be cleared on repeat in FileBox constructor
    }
    fpath = file_box->get_submitted_path();	// submitted, as set by user
#ifdef DEBUG
    printf ("BlendAlgebraFileButton::run file_box returned %d reinit_path=%d\n   fpath=%s\n",
	    result, file_box->reinit_path, fpath);
#endif
    strncpy (fname, fpath ? fpath : "", sizeof(fname)-1);
    delete file_box;
    file_box = 0;
    break;		// reinit_path remains cleared, exit loop
  }			// until reinit_path == 0

  gui->editing_lock->lock();
  if (result) gui->editing = 0;
  gui->editing_lock->unlock();
  if (! gui->editing) return;				// Cancel pressed

  if (fname[0])		// selected function name not empty, canonicalize it
  {	// if function is under project's dir, strip dir and make path relative
    strcpy (dir, plugin->server->mwindow->session->filename);
    // another project location might be plugin->server->mwindow->edl->path
    if (dir[0])		// project filename contains some path
    {
      cp = strrchr (dir, '/');
      if (cp)
      {
	cp[1] = 0;	// the directory of current project with trailing slash
	if (! strncmp (fname, dir, strlen(dir)))
	{
	  strcpy (dir, fname+strlen(dir));	// strip project dir off
	  strcpy (fname, dir+strspn(dir,"/"));	// ensure path is relative
	}
      }
    }
    if (strlen (fname) < 3 || strcmp (fname+strlen(fname)-3, ".ba"))
      strcat (fname, ".ba");	// suggest '.ba' suffix for blend functions
  }

  // Actualize selected function in config and in the main plugin dialog
  plugin->func_lock->lock("BlendAlgebraFileButton::run");
  strcpy (plugin->config.funcname, fname);
  BlendAlgebraTstamp = time(NULL);	// time of possible function change
#ifdef DEBUG
  printf ("BlendAlgebraFileButton::run setting function %s\n   timestamp %s",
	  plugin->config.funcname, ctime(&BlendAlgebraTstamp));
#endif
  plugin->func_lock->unlock();
  gui->lock_window("BlendAlgebraFileButton::run");
  gui->funcname->update(plugin->config.funcname);
  gui->unlock_window();
  gui->editing_lock->lock();
  gui->editing = 0;
  gui->editing_lock->unlock();

  plugin->send_configure_change();
}

void BlendAlgebraFileButton::stop()
{
  if (file_box) file_box->set_done(1);
  join();
}

BlendAlgebraFileBox::BlendAlgebraFileBox(BlendAlgebra *plugin,
					 BlendAlgebraWindow *gui,
					 char *init_path)
  : BC_FileBox(0, BC_WindowBase::get_resources()->filebox_h/2, init_path,
	       _("Blend Algebra: Select function source file"),"")
{
  this->plugin = plugin;
  this->gui = gui;

  to_curdir   = 0;
  to_usrlib   = 0;
  to_syslib   = 0;
  copy_curdir = 0;
  copy_usrlib = 0;
  file_edit   = 0;
  reinit_path = 0;	// reinit_path and changed_path can be set by buttons
  strcpy (changed_path, init_path);
}

BlendAlgebraFileBox::~BlendAlgebraFileBox()
{
}

// We need several additional buttons not foreseen in the bare FileBox.
// We arrange them in the place of (empty) FileBox caption.
void BlendAlgebraFileBox::add_objects()
{
  int xs10 = xS(10), xs5 = xS(5);
  int ys10 = yS(10);
  int x = xs10, y = ys10, x2;

  add_subwindow(to_curdir = new BlendAlgebraToCurdir(this, x, y));
  x2 = x+to_curdir->get_w()+xs5;
  add_subwindow(to_usrlib = new BlendAlgebraToUsrlib(this, x2, y));
  x2 += to_usrlib->get_w()+xs5;
  add_subwindow(to_syslib = new BlendAlgebraToSyslib(this, x2, y));
  y = get_y_margin();
  add_subwindow(copy_curdir = new BlendAlgebraCopyCurdir(this, x, y));
  x2 = x+copy_curdir->get_w()+xs5;
  add_subwindow(copy_usrlib = new BlendAlgebraCopyUsrlib(this, x2, y));
  x2 += copy_usrlib->get_w()+xs5;
  add_subwindow(file_edit = new BlendAlgebraFileEdit(this, x2, y));
  flush();
}

int BlendAlgebraFileBox::resize_event(int w, int h)
{
  int xs10 = xS(10), xs5 = xS(5);
  int x = xs10, y, x2;

  BC_FileBox::resize_event (w, h);

  y = get_y_margin();
  copy_curdir->reposition_window (x, y);
  x2 = x+copy_curdir->get_w()+xs5;
  copy_usrlib->reposition_window (x2, y);
  x2 += copy_usrlib->get_w()+xs5;
  file_edit->reposition_window (x2, y);

  flush();
  return 1;
}

BlendAlgebraToCurdir::BlendAlgebraToCurdir(BlendAlgebraFileBox *file_box,
					   int x, int y)
  : BC_GenericButton (x, y, _("=>Project"))
{
  this->file_box = file_box;
}

int BlendAlgebraToCurdir::handle_event()
{
  char *cp, fname[BCTEXTLEN], dir[BCTEXTLEN], path[BCTEXTLEN];

  strcpy (dir, file_box->plugin->server->mwindow->session->filename);
  if (dir[0])			// first get current project directory
  {
    cp = strrchr (dir, '/');
    if (cp) *cp = 0;
    else dir[0] = 0;
  }
  if (! dir[0])			// no project dir - get curdir as fallback
  {
    cp = getcwd (dir, sizeof(dir));
    if (! cp) dir[0] = 0;
  }
  if (! dir[0]) return 1;	// no curdir accessible, nothing to change

  fname[0] = 0;
  const char *spath = file_box->get_submitted_path();// get name entered so far
  if (spath) file_box->fs->extract_name (fname, spath);	// cut name from dir

  if (fname[0]) file_box->fs->join_names (path, dir, fname);
  else				// substitute old entered dir with project dir
  {
    strcpy (path, dir);
    strcat (path, "/");
  }

  // Not exactly sure what operations on FileBox are really important
  file_box->fs->change_dir (dir);	// force it to recognize the new dir

  // Changed path is in memory only, dialog text fields are not actualized.
  // file_box->refresh() does not help to refresh text fields either.
  // Therefore we have to apply a trick with closing FileBox and
  // reopening it with the new generated path. Visited paths history
  // will be updated inside BlendAlgebraFileButton::run() in reinit_path loop.
  strcpy (file_box->changed_path, path);

  file_box->reinit_path = 1;	// set flag to reopen FileBox afterwards
  file_box->set_done(1); // temporarily close FileBox, will be reopened later

  return 1;
}

BlendAlgebraToUsrlib::BlendAlgebraToUsrlib(BlendAlgebraFileBox *file_box,
					   int x, int y)
  : BC_GenericButton (x, y, _("=>Userlib"))
{
  this->file_box = file_box;
}

int BlendAlgebraToUsrlib::handle_event()
{
  char *cp, fname[BCTEXTLEN], dir[BCTEXTLEN], path[BCTEXTLEN];

  dir[0] = 0;
  cp = getenv ("CIN_USERLIB");		// $HOME/.bcast5lib by default
  if (cp) strcpy (dir, cp);
  if (! dir[0])
  {
    cp = getenv ("HOME");		// evtl resolve as default via $HOME
    if (cp) strcpy (dir, cp);
    if (dir[0]) strcat (dir, "/.bcast5lib");
    else
    {
      cp = getenv ("CIN_CONFIG");	// or via $CIN_CONFIG as fallback
      if (cp) strcpy (dir, cp);
      if (dir[0]) strcat (dir, "lib");
    }
  }
  if (! dir[0]) return 1;	// no user libdir known, nothing to change

  // The default user libdir for blend functions is $HOME/.bcast5lib/dlfcn/ba
  // Ensure it is a directory, evtl create dir, if not - do nothing else
  if (! file_box->fs->is_dir (dir)) file_box->fs->create_dir (dir);
  if (! file_box->fs->is_dir (dir)) return 1;
  strcat (dir, "/dlfcn");
  if (! file_box->fs->is_dir (dir)) file_box->fs->create_dir (dir);
  if (! file_box->fs->is_dir (dir)) return 1;
  strcat (dir, "/ba");
  if (! file_box->fs->is_dir (dir)) file_box->fs->create_dir (dir);
  if (! file_box->fs->is_dir (dir)) return 1;

  fname[0] = 0;
  const char *spath = file_box->get_submitted_path();// get name entered so far
  if (spath) file_box->fs->extract_name (fname, spath);	// cut name from dir
  if (fname[0]) file_box->fs->join_names (path, dir, fname);
  else				// substitute old entered dir with user libdir
  {
    strcpy (path, dir);
    strcat (path, "/");
  }

  // Reinitialize FileBox with the modified path
  file_box->fs->change_dir (dir);
  strcpy (file_box->changed_path, path);
  file_box->reinit_path = 1;	// set flag to reopen FileBox afterwards
  file_box->set_done(1); // temporarily close FileBox, will be reopened later

  return 1;
}

BlendAlgebraToSyslib::BlendAlgebraToSyslib(BlendAlgebraFileBox *file_box,
					   int x, int y)
  : BC_GenericButton (x, y, _("=>Syslib"))
{
  this->file_box = file_box;
}

int BlendAlgebraToSyslib::handle_event()
{
  char *cp, fname[BCTEXTLEN], dir[BCTEXTLEN], path[BCTEXTLEN];

  dir[0] = 0;
  cp = getenv ("CIN_DAT");	// Cinelerra installation directory (bin)
  if (cp) strcpy (dir, cp);
  if (! dir[0]) return 1;	// there is no default

  // System libdir for blend functions is $CIN_DAT/dlfcn/ba (bin/dlfcn/ba).
  // Ensure it is a directory, it must exist, if not - do nothing else
  strcat (dir, "/dlfcn/ba");
  if (! file_box->fs->is_dir (dir)) return 1;

  fname[0] = 0;
  const char *spath = file_box->get_submitted_path();// get name entered so far
  if (spath) file_box->fs->extract_name (fname, spath);	// cut name from dir
  if (fname[0]) file_box->fs->join_names (path, dir, fname);
  else				// substitute that old dir with system libdir
  {
    strcpy (path, dir);
    strcat (path, "/");
  }

  // Reinitialize FileBox with the modified path
  file_box->fs->change_dir (dir);
  strcpy (file_box->changed_path, path);
  file_box->reinit_path = 1;	// set flag to reopen FileBox afterwards
  file_box->set_done(1); // temporarily close FileBox, will be reopened later

  return 1;
}

BlendAlgebraCopyCurdir::BlendAlgebraCopyCurdir(BlendAlgebraFileBox *file_box,
					       int x, int y)
  : BC_GenericButton (x, y, _("Copy to project"))
{
  this->file_box = file_box;
}

int BlendAlgebraCopyCurdir::handle_event()
{
  int ret;
  char *cp, fname[BCTEXTLEN], dir[BCTEXTLEN], from_path[BCTEXTLEN],
    to_path[BCTEXTLEN], cmd[3*BCTEXTLEN];

  strcpy (dir, file_box->plugin->server->mwindow->session->filename);
  if (dir[0])			// first get current project directory
  {
    cp = strrchr (dir, '/');
    if (cp) *cp = 0;
    else dir[0] = 0;
  }
  if (! dir[0]) return 1;	// no curdir accessible, no copy target

  fname[0] = from_path[0] = 0;
  const char *spath = file_box->get_submitted_path();// get name entered so far
  if (spath)
  {
    strcpy (from_path, spath);			// this is copy source
    file_box->fs->extract_name (fname, spath);	// cut name from source dir
  }
  if (! (fname[0] && from_path[0])) return 1;	// no copy source ??

  file_box->fs->join_names (to_path, dir, fname);	// this is copy target

  if (! strcmp (from_path, to_path)) return 1;	// source and target identical

  if (file_box->fs->is_dir (from_path) || file_box->fs->is_dir (to_path))
    return 1;			// source and target must not be directories
  if (access (from_path, R_OK))
  {
    eprintf (_("Blend Algebra: source file %s does not exist or not readable\n"),
	     from_path);
    return 1;
  }
  if (! access (to_path, F_OK))
  {
    eprintf (_("Blend Algebra: target file %s exists, overwriting not allowed\n"),
	     to_path);
    return 1;
  }

  // Now do copy operation
  sprintf (cmd, "cp \"%s\" \"%s\"", from_path, to_path);
#ifdef DEBUG
  printf ("BlendAlgebraCopyCurdir::handle_event: executing %s\n", cmd);
#endif
  ret = system (cmd);
  if (ret)
  {
    eprintf (_("Blend Algebra: copying %s to %s failed\nsee console printout for diagnostics\n"),
	     from_path, to_path);
    return 1;
  }

  // Copying successful, now change dir to the location of the target
  file_box->fs->change_dir (dir);
  strcpy (file_box->changed_path, to_path);
  file_box->reinit_path = 1;	// set flag to reopen FileBox afterwards
  file_box->set_done(1); // temporarily close FileBox, will be reopened later

  return 1;
}

BlendAlgebraCopyUsrlib::BlendAlgebraCopyUsrlib(BlendAlgebraFileBox *file_box,
					       int x, int y)
  : BC_GenericButton (x, y, _("Copy to userlib"))
{
  this->file_box = file_box;
}

int BlendAlgebraCopyUsrlib::handle_event()
{
  int ret;
  char *cp, fname[BCTEXTLEN], dir[BCTEXTLEN], from_path[BCTEXTLEN],
    to_path[BCTEXTLEN], cmd[3*BCTEXTLEN];

  dir[0] = 0;
  cp = getenv ("CIN_USERLIB");		// $HOME/.bcast5lib by default
  if (cp) strcpy (dir, cp);
  if (! dir[0])
  {
    cp = getenv ("HOME");		// evtl resolve as default via $HOME
    if (cp) strcpy (dir, cp);
    if (dir[0]) strcat (dir, "/.bcast5lib");
    else
    {
      cp = getenv ("CIN_CONFIG");	// or via $CIN_CONFIG as fallback
      if (cp) strcpy (dir, cp);
      if (dir[0]) strcat (dir, "lib");
    }
  }
  if (! dir[0]) return 1;	// no user libdir known, no copy target

  // Evtl create user libdir, if not successful - do nothing else
  if (! file_box->fs->is_dir (dir)) file_box->fs->create_dir (dir);
  if (! file_box->fs->is_dir (dir)) return 1;
  strcat (dir, "/dlfcn");
  if (! file_box->fs->is_dir (dir)) file_box->fs->create_dir (dir);
  if (! file_box->fs->is_dir (dir)) return 1;
  strcat (dir, "/ba");
  if (! file_box->fs->is_dir (dir)) file_box->fs->create_dir (dir);
  if (! file_box->fs->is_dir (dir)) return 1;

  fname[0] = from_path[0] = 0;
  const char *spath = file_box->get_submitted_path();// get name entered so far
  if (spath)
  {
    strcpy (from_path, spath);			// this is copy source
    file_box->fs->extract_name (fname, spath);	// cut name from source dir
  }
  if (! (fname[0] && from_path[0])) return 1;	// no copy source ??

  file_box->fs->join_names (to_path, dir, fname);	// this is copy target

  if (! strcmp (from_path, to_path)) return 1;	// source and target identical

  if (file_box->fs->is_dir (from_path) || file_box->fs->is_dir (to_path))
    return 1;			// source and target must not be directories
  if (access (from_path, R_OK))
  {
    eprintf (_("Blend Algebra: source file %s does not exist or not readable\n"),
	     from_path);
    return 1;
  }
  if (! access (to_path, F_OK))
  {
    eprintf (_("Blend Algebra: target file %s exists, overwriting not allowed\n"),
	     to_path);
    return 1;
  }

  // Now do copy operation
  sprintf (cmd, "cp \"%s\" \"%s\"", from_path, to_path);
#ifdef DEBUG
  printf ("BlendAlgebraCopyUsrlib::handle_event: executing %s\n", cmd);
#endif
  ret = system (cmd);
  if (ret)
  {
    eprintf (_("Blend Algebra: copying %s to %s failed\nsee console printout for diagnostics\n"),
	     from_path, to_path);
    return 1;
  }

  return 1; // Copying successful, but don't change directory to user libdir
}

BlendAlgebraFileEdit::BlendAlgebraFileEdit(BlendAlgebraFileBox *file_box,
					   int x, int y)
  : BC_GenericButton (x, y, _("Edit..."))
{
  this->file_box = file_box;
}

int BlendAlgebraFileEdit::handle_event()
{
  char fname[BCTEXTLEN], dir[BCTEXTLEN], str[2*BCTEXTLEN];

  fname[0] = 0;
  const char *spath = file_box->get_submitted_path();// get name entered so far
  if (spath) strcpy (fname, spath);
  if (! fname[0])
  {
    eprintf (_("Blend Algebra: no function to edit, select source file first\n"));
    return 1;
  }

  // Evtl make function path absolute by prepending project path to it
  if (fname[0] != '/')	// fname is relative, prepend current project path
  {
    strcpy (dir, file_box->plugin->server->mwindow->session->filename);
    if (dir[0])
    {
      char *cp = strrchr (dir, '/');
      if (cp)
      {
	cp[1] = 0;		// strip project filename off from project path
	strcat (dir, fname);	// concatenate obtained path with function name
	strcpy (fname, dir);
      }
    }
  }
  if (file_box->fs->is_dir (fname))
  {
    eprintf (_("Blend Algebra: cannot edit directory, select source file first\n"));
    return 1;
  }

  // This will run configured external editor via perl script
  // If editor start is not backgrounded, GUI will block until editor exits
  sprintf(str, "\"%s/dlfcn/BlendAlgebraCompile.pl\" -edit \"%s\"",
	  getenv("CIN_DAT"), fname);
#ifdef DEBUG
  printf ("BlendAlgebraFileEdit::handle_event: executing:\n   %s\n", str);
#endif
  system (str);				// runs configured external editor

  file_box->plugin->func_lock->lock("BlendAlgebraFileEdit::handle_event");
  BlendAlgebraTstamp = time(NULL);	// force refresh of dlopen'd functions
#ifdef DEBUG
  printf ("BlendAlgebraFileEdit::handle_event edited function %s\n   timestamp %s",
	  fname, ctime(&BlendAlgebraTstamp));
#endif
  file_box->plugin->func_lock->unlock();

  return 1;
}

BlendAlgebraClipcolors::BlendAlgebraClipcolors(BlendAlgebra *plugin,
					       BlendAlgebraWindow *gui,
					       int x, int y)
  : BC_CheckBox(x, y, plugin->config.clipcolors)
{
  this->plugin = plugin;
  this->gui = gui;
}

int BlendAlgebraClipcolors::handle_event()
{
  plugin->config.clipcolors = get_value();
  plugin->send_configure_change();
  return 1;
}

BlendAlgebraParallel::BlendAlgebraParallel(BlendAlgebra *plugin,
					   BlendAlgebraWindow *gui,
					   int x, int y)
  : BC_CheckBox(x, y, plugin->config.parallel)
{
  this->plugin = plugin;
  this->gui = gui;
}

int BlendAlgebraParallel::handle_event()
{
  plugin->config.parallel = get_value();
  plugin->send_configure_change();
  return 1;
}

BlendAlgebraClearInput::BlendAlgebraClearInput(BlendAlgebra *plugin,
					       BlendAlgebraWindow *gui,
					       int x, int y)
  : BC_CheckBox(x, y, plugin->config.clear_input)
{
  this->plugin = plugin;
  this->gui = gui;
}

int BlendAlgebraClearInput::handle_event()
{
  plugin->config.clear_input = get_value();
  plugin->send_configure_change();
  return 1;
}

BlendAlgebraDirection::BlendAlgebraDirection(BlendAlgebra *plugin, int x, int y)
  : BC_PopupMenu(x, y, xS(150),
	BlendAlgebraConfig::direction_to_text(plugin->config.direction), 1)
{
  this->plugin = plugin;
}

void BlendAlgebraDirection::create_objects()
{
  add_item(new BC_MenuItem(BlendAlgebraConfig::direction_to_text(
			     BlendAlgebraConfig::TOP_FIRST)));
  add_item(new BC_MenuItem(BlendAlgebraConfig::direction_to_text(
			     BlendAlgebraConfig::BOTTOM_FIRST)));
}

int BlendAlgebraDirection::handle_event()
{
  char *text = get_text();

  if(!strcmp(text, BlendAlgebraConfig::direction_to_text(
	       BlendAlgebraConfig::TOP_FIRST)))
    plugin->config.direction = BlendAlgebraConfig::TOP_FIRST;
  else if(!strcmp(text, BlendAlgebraConfig::direction_to_text(
		    BlendAlgebraConfig::BOTTOM_FIRST)))
    plugin->config.direction = BlendAlgebraConfig::BOTTOM_FIRST;

  plugin->send_configure_change();
  return 1;
}

BlendAlgebraOutput::BlendAlgebraOutput(BlendAlgebra *plugin, int x, int y)
  : BC_PopupMenu(x, y, xS(100),
	BlendAlgebraConfig::output_to_text(plugin->config.output_track), 1)
{
  this->plugin = plugin;
}

void BlendAlgebraOutput::create_objects()
{
  add_item(new BC_MenuItem(BlendAlgebraConfig::output_to_text(
			     BlendAlgebraConfig::TOP)));
  add_item(new BC_MenuItem(BlendAlgebraConfig::output_to_text(
			     BlendAlgebraConfig::BOTTOM)));
}

int BlendAlgebraOutput::handle_event()
{
  char *text = get_text();

  if(!strcmp(text, BlendAlgebraConfig::output_to_text(
	       BlendAlgebraConfig::TOP)))
    plugin->config.output_track = BlendAlgebraConfig::TOP;
  else if(!strcmp(text, BlendAlgebraConfig::output_to_text(
		    BlendAlgebraConfig::BOTTOM)))
    plugin->config.output_track = BlendAlgebraConfig::BOTTOM;

  plugin->send_configure_change();
  return 1;
}

BlendAlgebraColorspace::BlendAlgebraColorspace(BlendAlgebra *plugin,
					       int x, int y)
  : BC_PopupMenu(x, y, xS(150),
	BlendAlgebraConfig::colorspace_to_text(plugin->config.colorspace), 1)
{
  this->plugin = plugin;
}

void BlendAlgebraColorspace::create_objects()
{
  add_item(new BC_MenuItem(BlendAlgebraConfig::colorspace_to_text(
			     BlendAlgebraConfig::AUTO)));
  add_item(new BC_MenuItem(BlendAlgebraConfig::colorspace_to_text(
			     BlendAlgebraConfig::RGB)));
  add_item(new BC_MenuItem(BlendAlgebraConfig::colorspace_to_text(
			     BlendAlgebraConfig::YUV)));
  add_item(new BC_MenuItem(BlendAlgebraConfig::colorspace_to_text(
			     BlendAlgebraConfig::HSV)));
  add_item(new BC_MenuItem(BlendAlgebraConfig::colorspace_to_text(
			     BlendAlgebraConfig::PROJECT)));
}

int BlendAlgebraColorspace::handle_event()
{
  char *text = get_text();

  if(!strcmp(text, BlendAlgebraConfig::colorspace_to_text(
	       BlendAlgebraConfig::AUTO)))
    plugin->config.colorspace = BlendAlgebraConfig::AUTO;
  else if(!strcmp(text, BlendAlgebraConfig::colorspace_to_text(
		    BlendAlgebraConfig::RGB)))
    plugin->config.colorspace = BlendAlgebraConfig::RGB;
  else if(!strcmp(text, BlendAlgebraConfig::colorspace_to_text(
		    BlendAlgebraConfig::YUV)))
    plugin->config.colorspace = BlendAlgebraConfig::YUV;
  else if(!strcmp(text, BlendAlgebraConfig::colorspace_to_text(
		    BlendAlgebraConfig::HSV)))
    plugin->config.colorspace = BlendAlgebraConfig::HSV;
  else if(!strcmp(text, BlendAlgebraConfig::colorspace_to_text(
		    BlendAlgebraConfig::PROJECT)))
    plugin->config.colorspace = BlendAlgebraConfig::PROJECT;

  plugin->send_configure_change();
  return 1;
}

BlendAlgebraKeyColor::BlendAlgebraKeyColor (BlendAlgebra *plugin,
					    BlendAlgebraWindow *gui,
					    int x, int y)
  : BC_GenericButton (x, y, _("Select key color..."))
{
  this->plugin = plugin;
  this->gui = gui;
}

int BlendAlgebraKeyColor::handle_event()
{
  gui->color_thread->start_window (plugin->config.get_key_color(), 0xff);
  return 1;
}

BlendAlgebraColorPicker::BlendAlgebraColorPicker (BlendAlgebra *plugin,
						  BlendAlgebraWindow *gui,
						  int x, int y)
  : BC_GenericButton (x, y, _("Get from color picker"))
{
  this->plugin = plugin;
  this->gui = gui;
}

int BlendAlgebraColorPicker::handle_event()
{
  plugin->config.red   = plugin->get_red();
  plugin->config.green = plugin->get_green();
  plugin->config.blue  = plugin->get_blue();

  gui->update_key_sample();

  plugin->send_configure_change();
  return 1;
}

BlendAlgebraColorThread::BlendAlgebraColorThread (BlendAlgebra * plugin,
						  BlendAlgebraWindow * gui)
  : ColorPicker (0, _("Select color"))
{
  this->plugin = plugin;
  this->gui = gui;
}

int BlendAlgebraColorThread::handle_new_color (int output, int alpha)
{
  plugin->config.red   = (float) ((output & 0xff0000) >> 16) / 255;
  plugin->config.green = (float) ((output & 0x00ff00) >>  8) / 255;
  plugin->config.blue  = (float) ((output & 0x0000ff)      ) / 255;

  get_gui()->unlock_window();
  gui->lock_window("BlendAlgebraColorThread::handle_new_color");
  gui->update_key_sample();
  gui->unlock_window();
  get_gui()->lock_window("BlendAlgebraColorThread::handle_new_color");

  plugin->send_configure_change();
  return 1;
}

BlendAlgebraAlphaText::BlendAlgebraAlphaText(BlendAlgebra *plugin,
					     BlendAlgebraWindow *gui,
					     BlendAlgebraAlphaSlider *slider,
					     int x, int y,
					     float min, float max,
					     float *output)
  : BC_TumbleTextBox(gui, *output, min, max, x, y, xS(60), 2)
{
  this->plugin = plugin;
  this->gui = gui;
  this->slider = slider;
  this->min = min;
  this->max = max;
  this->output = output;
  set_increment(0.01);
}

BlendAlgebraAlphaText::~BlendAlgebraAlphaText()
{
}

int BlendAlgebraAlphaText::handle_event()
{
  *output = atof(get_text());
  if(*output > max) *output = max;
  if(*output < min) *output = min;
  slider->update(*output);

  plugin->send_configure_change();
  return 1;
}

BlendAlgebraAlphaSlider::BlendAlgebraAlphaSlider(BlendAlgebra *plugin,
						 BlendAlgebraAlphaText *text,
						 int x, int y, int w,
						 float min, float max,
						 float *output)
  : BC_FSlider(x, y, 0, w, w, min, max, *output)
{
  this->plugin = plugin;
  this->text = text;
  this->output = output;
  set_precision(0.01);
  enable_show_value(0);		// Hide caption
}

BlendAlgebraAlphaSlider::~BlendAlgebraAlphaSlider()
{
}

int BlendAlgebraAlphaSlider::handle_event()
{
  *output = get_value();
  text->update(*output);

  plugin->send_configure_change();
  return 1;
}

BlendAlgebraWindow::BlendAlgebraWindow(BlendAlgebra *plugin)
  : PluginClientWindow(plugin, xS(450), yS(410), xS(450), yS(410), 0)
{
  this->plugin = plugin;
  color_thread = 0;
  editing_lock = new Mutex("BlendAlgebraWindow::editing_lock");
  editing  = 0;
  quit_now = 0;
}

BlendAlgebraWindow::~BlendAlgebraWindow()
{
  quit_now = 1;	// cleanup in progress, stop mocking up with editing_lock
  if (color_thread) color_thread->close_window();
  file_button->stop();			// force closing Attach... dialog
  editing = 0;
  delete color_thread;
  delete editing_lock;
}

void BlendAlgebraWindow::create_objects()
{
  int xs5 = xS(5), xs10 = xS(10), xs20 = xS(20);
  int ys5 = yS(5), ys10 = yS(10), ys20 = yS(20), ys30 = yS(30), ys40 = yS(40);
  int x = xs10, y = ys10, x2;
  BC_Title *title;
  BC_TitleBar *title_bar;

  // Programming section
  add_subwindow (title_bar =
		 new BC_TitleBar(x, y, get_w()-2*x, xs20, xs10,
				 _("Blend programming environment")));

  y += ys30;
  add_subwindow(title = new BC_Title(x, y, _("Function:")));
  add_subwindow(funcname =
		new BlendAlgebraFuncname(plugin, plugin->config.funcname, this,
					 x + title->get_w() + xs5, y));

  y += ys30;
  add_subwindow(file_button = new BlendAlgebraFileButton(plugin, this, x, y));
  x2 = x+file_button->get_w()+xs5;
  add_subwindow(edit_button = new BlendAlgebraEdit(plugin, this, x2, y));
  x2 += edit_button->get_w()+xs5;
  add_subwindow(refresh_button = new BlendAlgebraRefresh(plugin, this, x2, y));
  x2 += refresh_button->get_w()+xs5;
  add_subwindow(detach_button = new BlendAlgebraDetach(plugin, this, x2, y));

  y += ys30;
  add_subwindow(title = new BC_Title(x, y, _("Color space:")));
  add_subwindow(colorspace =
		new BlendAlgebraColorspace(plugin,
					   x + title->get_w() + xs5, y));
  colorspace->create_objects();

  x2 = x+title->get_w()+colorspace->get_w()+xs10+xs10;
  add_subwindow(title = new BC_Title(x2, y, _("Parallelize processing")));
  add_subwindow(parallel =
		new BlendAlgebraParallel(plugin, this,
					 x2 + title->get_w() + xs5, y));

  // Supplementary color section
  y += ys40;
  add_subwindow (title_bar =
		 new BC_TitleBar(x, y, get_w()-2*x, xs20, xs10,
				 _("Supplementary color selection")));

  y += ys30;
  add_subwindow(title =
		new BC_Title(x, y, _("Chroma key or substitution color:")));
  add_subwindow(key_sample = new BC_SubWindow(x + title->get_w() + xs5, y,
					      xS(150), yS(50)));
  y += ys20+ys5;
  add_subwindow(title = new BC_Title(x, y, _("Clip color values")));
  add_subwindow(clipcolors =
		new BlendAlgebraClipcolors(plugin, this,
					   x + title->get_w() + xs5, y));

  y += ys30+ys5;
  add_subwindow(key_color = new BlendAlgebraKeyColor(plugin, this, x, y));
  x2 = x+key_color->get_w()+xs5;
  add_subwindow(color_picker =
		new BlendAlgebraColorPicker(plugin, this, x2, y));

  y += ys30+ys5;
  add_subwindow(title = new BC_Title(x, y, _("Substitution opacity:")));
  alpha_text = new BlendAlgebraAlphaText (plugin, this, 0,
					  x+title->get_w()+xs10+xs5+xS(210),
					  y, 0, 1, &plugin->config.alpha);
  alpha_text->create_objects();
  key_alpha = new BlendAlgebraAlphaSlider (plugin, alpha_text,
					   x + title->get_w() + xs5, y,
					   xS(210), 0, 1,
					   &plugin->config.alpha);
  add_subwindow(key_alpha);
  alpha_text->slider = key_alpha;

  // Track arrangement section
  y += ys40;
  add_subwindow (title_bar =
		 new BC_TitleBar(x, y, get_w()-2*x, xs20, xs10,
				 _("Processed tracks arrangement")));

  y += ys30;
  add_subwindow(title = new BC_Title(x, y, _("Track order:")));
  add_subwindow(direction =
		new BlendAlgebraDirection(plugin,
					  x + title->get_w() + xs5, y));
  direction->create_objects();
  x2 = x+title->get_w()+direction->get_w()+xs10;
  add_subwindow(title = new BC_Title(x2, y, _("Output track:")));
  add_subwindow(output =
		new BlendAlgebraOutput(plugin, x2 + title->get_w() + xs5, y));
  output->create_objects();

  y += ys30;
  add_subwindow(title =
		new BC_Title(x, y,
			     _("Hide input tracks, use output exclusively")));
  add_subwindow(clear_input =
		new BlendAlgebraClearInput(plugin, this,
					   x + title->get_w() + xs5, y));

  color_thread = new BlendAlgebraColorThread(plugin, this);

  update_key_sample();
  show_window();
  flush();
}

void BlendAlgebraWindow::update_key_sample()
{
  key_sample->set_color (plugin->config.get_key_color());
  key_sample->draw_box (0, 0, key_sample->get_w(), key_sample->get_h());
  key_sample->set_color (BLACK);
  key_sample->draw_rectangle (0, 0, key_sample->get_w(), key_sample->get_h());
  key_sample->flash ();
}

void BlendAlgebraWindow::done_event()
{
  color_thread->close_window();
  file_button->stop();
}

int BlendAlgebraWindow::close_event()
{
  color_thread->close_window();
  file_button->stop();
  set_done(1);
  return 1;
}

int BlendAlgebraWindow::hide_window (int flush)
{
  color_thread->close_window();
  file_button->stop();
  return BC_WindowBase::hide_window (flush);
}

////////////////////////////////////////////
// Plugin main class implementation
////////////////////////////////////////////

BlendAlgebraFunc::BlendAlgebraFunc()
{
  src[0] =  0;
  handle =  0;
  proc   =  0;
  init   =  0;
  tstamp = -1;
}

BlendAlgebraFunc::~BlendAlgebraFunc()
{
  if (handle)
  {
#ifdef DEBUG
    printf ("BlendAlgebraFunc destructor detaching function dlclose(%s)\n",
	    src);
#endif
    dlclose (handle);
  }
}

BlendAlgebra::BlendAlgebra(PluginServer *server)
  : PluginVClient(server)
{
  BlendAlgebraTstamp = time(NULL);
  inspect_configuration = 1;			// force initial configuration
  curr_func_no = -1;
  func_lock = new Mutex("BlendAlgebra::func_lock");
  engine = 0;
#ifdef DEBUG
  printf ("BlendAlgebra constructor timestamp %s", ctime(&BlendAlgebraTstamp));
#endif
}

BlendAlgebra::~BlendAlgebra()
{
  if (engine) delete engine;
  delete func_lock;
#ifdef DEBUG
  printf ("BlendAlgebra destructor removing all %d functions\n",
	  funclist.total);
#endif
  funclist.remove_all_objects();
  curr_func.handle = 0;
}

int BlendAlgebra::process_buffer(VFrame **frame,
				 int64_t start_position,
				 double frame_rate)
{
  BlendAlgebraFunc *ptr;
  int refresh_eprintf = 0;

  // Mocking up with function shared object if it might get modified
  // Not sure if plugin locking is needed for this separate processing instance
  func_lock->lock("BlendAlgebra::process_buffer");

  // First check if function name was changed
  if (load_configuration() || inspect_configuration) // function might change
  {
    inspect_configuration = 0;	// do once after change or after creation
    if (strcmp (curr_func.src, config.funcname))	// function changed
    {
      curr_func.src[0] = 0;
      curr_func.handle = 0;
      curr_func.proc   = 0;
      curr_func.init   = 0;
      curr_func.tstamp = -1;
      curr_func_no = -1;
      if (config.funcname[0])			// function name not empty
      {
	refresh_eprintf = 1;			// probably new function
	strcpy (curr_func.src, config.funcname);
#ifdef DEBUG
	printf ("BlendAlgebra::process_buffer searching function %s out of %d\n",
		curr_func.src, funclist.total);
#endif
	for (int i=0; i<funclist.total; i++)	// cache of linked functions
	{
	  ptr = funclist[i];
	  if (! strcmp (curr_func.src, ptr->src)) // cached function found
	  {
	    curr_func.handle = ptr->handle;
	    curr_func.proc   = ptr->proc;
	    curr_func.init   = ptr->init;
	    curr_func.tstamp = ptr->tstamp;
	    curr_func_no = i;
#ifdef DEBUG
	    printf ("BlendAlgebra::process_buffer cached function %s found: %d\n   timestamp %s",
		    ptr->src, curr_func_no, ctime(&ptr->tstamp));
#endif
	    break;
	  }					// if cached function found
	}					// for funclist.total
      }						// if function name not empty
    }						// if function changed
  }						// if load_configuration()

  // Now ensure that function binary is up to date, evtl recompile/relink it
  if (curr_func.src[0])				// current function not empty
  {
    if (curr_func.tstamp == -1 || BlendAlgebraTstamp > curr_func.tstamp)
    {		// function first seen or linked before last config change
      char str[BCTEXTLEN*2], dir[BCTEXTLEN], path[BCTEXTLEN];
      time_t tstamp = -1;

      // Evtl make function path absolute by prepending project path to it
      strcpy (path, curr_func.src);
      if (path[0] != '/') // path is relative, prepend current project path
      {
	strcpy (dir, server->mwindow->session->filename);
	if (dir[0])
	{
	  char *cp = strrchr (dir, '/');
	  if (cp)
	  {
	    cp[1] = 0;		// strip project filename off from project path
	    strcat (dir, path);	// concatenate obtained path with function name
	    strcpy (path, dir);
	  }
	}
      }

      // Try to lock function filename against concurrent compiler runs
      // This kind of locking seems definitely reasonable here
      struct flock locks;
      locks.l_whence = SEEK_SET;
      locks.l_start = locks.l_len = 0;
      int fd = open (path, O_RDWR);
      if (fd > -1)		// if lock cannot be set, ignore this for now
      {
	locks.l_type = F_WRLCK;
	fcntl (fd, F_SETLKW, &locks); // try to wait for lock, ignoring errors
	sprintf(str, "\"%s/dlfcn/BlendAlgebraCompile.pl\" \"%s\"",
		getenv("CIN_DAT"), path);
#ifdef DEBUG
	printf ("BlendAlgebra::process_buffer\n   curr_func.tstamp %s",
		ctime(&curr_func.tstamp));
	printf ("   global tstamp %s   executing %s\n",
		ctime(&BlendAlgebraTstamp), str);
#endif
	system (str);	// evtl recompile function if source newer than object
	if (path[0] == '/') sprintf (str, "%s.so", path);
	else sprintf (str, "./%s.so", path);	// dlopen requires a slash
	struct stat statbuf;
	if (0 == stat (str, &statbuf)) tstamp = statbuf.st_mtime;
      }
      else				// function source cannot be opened
      {
#ifdef DEBUG
	printf ("BlendAlgebra::process_buffer cannot access function %s\n",
		curr_func.src);
#endif
      }

      // Now test if function relinking needed, make function cache consistent
      if (tstamp == -1)
      {		// either function does not exist or compilation unsuccessful
	if (fd > -1)
	  eprintf (_("Blend Algebra: compilation of function %s failed\nsee console printout for diagnostics\n"),
		   curr_func.src);
	if (curr_func_no >= 0)
	{						// detach old function
	  if (funclist[curr_func_no]->handle)
	  {
#ifdef DEBUG
	    printf ("BlendAlgebra::process_buffer detaching function %d dlclose(%s)\n",
		    curr_func_no, funclist[curr_func_no]->src);
#endif
	    dlclose (funclist[curr_func_no]->handle);
	  }
#ifdef DEBUG
	  printf ("BlendAlgebra::process_buffer removing function %d (%s)\n",
		  curr_func_no, curr_func.src);
#endif
	  funclist[curr_func_no]->src[0] = 0;
	  funclist[curr_func_no]->handle = 0;
	  funclist[curr_func_no]->proc   = 0;
	  funclist[curr_func_no]->init   = 0;
	  funclist[curr_func_no]->tstamp = -1;
	  funclist.remove_object_number (curr_func_no);
	  curr_func_no = -1;
	}
	curr_func.handle = 0;
	curr_func.proc   = 0;
	curr_func.init   = 0;
	curr_func.tstamp = time (NULL);
      }
      else if (curr_func.tstamp == -1 || tstamp > curr_func.tstamp)
      {				// function first seen or edited after linkage
#ifdef DEBUG
	printf ("BlendAlgebra::process_buffer\n   curr_func.tstamp %s",
		ctime(&curr_func.tstamp));
	printf ("   tstamp %s   relinking %s\n", ctime(&tstamp), str);
#endif
	if (curr_func_no >= 0 && funclist[curr_func_no]->handle)
	{						// detach old function
#ifdef DEBUG
	  printf ("BlendAlgebra::process_buffer detaching function %d dlclose(%s)\n",
		  curr_func_no, funclist[curr_func_no]->src);
#endif
	  dlclose (funclist[curr_func_no]->handle);
	  funclist[curr_func_no]->src[0] = 0;
	  funclist[curr_func_no]->handle = 0;
	  funclist[curr_func_no]->proc   = 0;
	  funclist[curr_func_no]->init   = 0;
	  funclist[curr_func_no]->tstamp = -1;
	}
	curr_func.proc   = 0;
	curr_func.init   = 0;
	curr_func.handle = dlopen (str, RTLD_NOW);	// shared object handle
#ifdef DEBUG
	printf ("BlendAlgebra::process_buffer dlopen(%s)=%p\n",
		str, curr_func.handle);
#endif
	if (curr_func.handle)	// inquire necessary extern entry points
	{			// baProc is mandatory, baInit optional
	  curr_func.init = (BAF_init) dlsym (curr_func.handle, "baInit");
	  if (curr_func.init == NULL)	// not a problem, we can continue
	    printf (_("Blend Algebra: optional entry point \"baInit\" for function %s not found:\n%s\n"),
		    str, dlerror());
	  curr_func.proc = (BAF_proc) dlsym (curr_func.handle, "baProc");
#ifdef DEBUG
	  printf ("BlendAlgebra::process_buffer dlsym(%s) init=%p proc=%p\n",
		  curr_func.src, curr_func.init, curr_func.proc);
#endif
	  if (curr_func.proc == NULL)	// nothing to do if this not working
	  {
	    eprintf (_("Blend Algebra: entry point \"baProc\" for function %s not found:\n%s\n"),
		     str, dlerror());
#ifdef DEBUG
	    printf ("BlendAlgebra::process_buffer dlclose(%s)\n",
		    curr_func.src);
#endif
	    dlclose (curr_func.handle);
	    curr_func.handle = 0;
	    curr_func.init   = 0;
	  }
	}
	else
	  eprintf (_("Blend Algebra: dynamic load of function %s failed:\n%s\n"),
		   str, dlerror());
	curr_func.tstamp = time (NULL);
	if (curr_func_no >= 0)		// function was already in cache
	{
	  if (curr_func.proc)			// update object in cache
	  {
	    strcpy (funclist[curr_func_no]->src, curr_func.src);
	    funclist[curr_func_no]->handle = curr_func.handle;
	    funclist[curr_func_no]->proc   = curr_func.proc;
	    funclist[curr_func_no]->init   = curr_func.init;
	    funclist[curr_func_no]->tstamp = curr_func.tstamp;
#ifdef DEBUG
	    printf ("BlendAlgebra::process_buffer function %d (%s) updated\n   timestamp %s",
		    curr_func_no, curr_func.src, ctime(&curr_func.tstamp));
#endif
	  }
	  else					// remove outdated function
	  {
#ifdef DEBUG
	    printf ("BlendAlgebra::process_buffer removing function %d (%s)\n",
		    curr_func_no, curr_func.src);
#endif
	    funclist.remove_object_number (curr_func_no);
	    curr_func_no = -1;
	  }
	}
	else if (curr_func.proc)	// add new linked function to cache
	{
	  curr_func_no = funclist.total;
	  ptr = new BlendAlgebraFunc;
	  funclist.append (ptr);
	  strcpy (ptr->src, curr_func.src);
	  ptr->handle = curr_func.handle;
	  ptr->proc   = curr_func.proc;
	  ptr->init   = curr_func.init;
	  ptr->tstamp = curr_func.tstamp;
#ifdef DEBUG
	  printf ("BlendAlgebra::process_buffer function %d (%s) appended\n   timestamp %s",
		  curr_func_no, ptr->src, ctime(&ptr->tstamp));
#endif
	}				// if function in cache
      }
      else				// function does not need relinking
      {
	curr_func.tstamp = time (NULL);
	if (curr_func_no >= 0)			// just update timestamp
	  funclist[curr_func_no]->tstamp = curr_func.tstamp;
#ifdef DEBUG
	printf ("BlendAlgebra::process_buffer function %s does not need relinking\n   cache number %d timestamp %s",
		curr_func.src, curr_func_no, ctime(&curr_func.tstamp));
#endif
      }						// if tstamp

      if (fd > -1)				// unlock function
      {
	locks.l_type = F_UNLCK;
	fcntl (fd, F_SETLK, &locks);
	close (fd);
      }
    }					// if function first seen or changed
  }					// if current function not empty

  func_lock->unlock();		// end mocking up with function shared object

  // Now prepare the important pars and read all involved frames...
  layers = get_total_buffers();
  width  = frame[0]->get_w();
  height = frame[0]->get_h();
  for (int l=0; l<layers; l++)
    read_frame (frame[l], l, start_position, frame_rate, 0);
  this->frame = frame;

  if (curr_func.proc == NULL) return 0;		// no function, nothing to do

  color_proj = frame[0]->get_color_model(); // internal colorspace of project
  if (color_proj == BC_RGBA_FLOAT ||
      color_proj == BC_RGBA8888   ||
      color_proj == BC_YUVA8888)
    has_alpha = 1;			// has alpha channel
  else has_alpha = 0;
  color_work = config.colorspace;	// will be requested from the function
  int color_arg  = color_work;
  int min_layers = layers;		// function's min required no of tracks
  int parallel   = 0;			// assumed not parallelized by default
  if (curr_func.init != NULL)		// ask function about important pars
    curr_func.init (&color_arg, color_proj, &min_layers, layers,
		    &parallel, config.parallel, width, height, has_alpha);
  if (min_layers > layers)
  {
    if (refresh_eprintf)
      eprintf (_("Blend Algebra: cannot execute function %s:\nrequires %d tracks to process, has only %d tracks\n"),
	       curr_func.src, min_layers, layers);
    return 0;				// too few tracks to do anything
  }
  if (color_work == BlendAlgebraConfig::AUTO) color_work = color_arg;
  if (color_work == BlendAlgebraConfig::AUTO)
    color_work = BlendAlgebraConfig::PROJECT; // still not defined, dont change
  if (! config.parallel) parallel = 0;		// parallelism not requested

  // In case of a fatal bug in the user defined function (SIGFPE, SIGSEGV,...)
  // Cinelerra perhaps will crash. Unfortunately we cannot handle the signals
  // here: signal handler as set by sigaction() is process-wide, the same
  // for all threads. And Cinelerra already uses its own signal handler
  // for debug purposes which we are not allowed to overwrite with our one.
  // Infinities and NaN will be trapped and substituted with configured color.
  // Here we prepare key color components in three possible color spaces
  // from this configured color. Used to substitute NaN or infinities.
  // Can be used also inside user's function like a chroma key.
  rgb_r = config.red;		// user's configured color is always RGB
  rgb_g = config.green;
  rgb_b = config.blue;
  YUV::yuv.rgb_to_yuv_f (rgb_r, rgb_g, rgb_b, yuv_y, yuv_u, yuv_v); // make YUV
  HSV::rgb_to_hsv       (rgb_r, rgb_g, rgb_b, hsv_h, hsv_s, hsv_v); // make HSV
  key_a = config.alpha;		// user's configured alpha

  if (parallel)		// parallelism desired, and supoported by the function
  {
    if (! engine)
      engine = new BlendAlgebraEngine (this,
				       get_project_smp() + 1,
				       get_project_smp() + 1);
    engine->process_packages();
  }
  else process_frames (0, height);	// process everything sequential

  return 0;						// WHEW !!!
}

// Now comes the whole math. User's function will get everything in float.
// If the project's color model is 8-bit, pixels will be converted to float.
// Then, if requested, pixels will be converted to working color space
// (RGB, YUV, or HSV) which is required by the function. After processing,
// all the conversions will be rolled back in the reverse order for the result.
// This universal function is called via loadbalance multithreading engine
// as well as directly if parallelism not requested or not supported

void BlendAlgebra::process_frames (int y1, int y2)
{
  float r[layers], g[layers], b[layers], a[layers];
  float rk, gk, bk, yk, uk, vk, out_r, out_g, out_b, out_a;
  int k, l, start, step, arg_out, trk_out;

  // start, step define function argument indices relative to track numbers
  // trk_out defines result index relative to track numbers
  // arg_out defines result index relative to function args (start, step)
  // arg_out used before function call to preinitialize future result
  // trk_out used after function call to place result into the right track
  // if function does not set the result, output track stays unmodified
  if (config.direction == BlendAlgebraConfig::BOTTOM_FIRST)
  {
    start = layers-1;
    step  = -1;
    if (config.output_track == BlendAlgebraConfig::TOP) arg_out = layers-1;
    else arg_out = 0;
  }
  else
  {
    start = 0;
    step  = 1;
    if (config.output_track == BlendAlgebraConfig::TOP) arg_out = 0;
    else arg_out = layers-1;
  }
  if (config.output_track == BlendAlgebraConfig::TOP) trk_out = 0;
  else trk_out = layers-1;

  int clip_colors = config.clipcolors;		// clipping floats is optional
  yk = uk = vk = 0;				// to make gcc -O2 happy

  switch (color_proj)
  {
  case BC_RGB_FLOAT:		// RGB  [ 0 .. 1 ], out of bounds possible
  case BC_RGBA_FLOAT:		// RGBA [ 0 .. 1 ], out of bounds possible
    for (int i=y1; i<y2; i++)		// scan all rows
    {
      float *row[layers];
      for (l=0; l<layers; l++) row[l] = (float *)frame[l]->get_rows()[i];
      for (int j=0; j<width; j++)	// scan all pixels
      {
	k = start;
	for (l=0; l<layers; l++)	// convert source frames to args
	{
	  if (color_work == BlendAlgebraConfig::YUV)
	  {
	    YUV::yuv.rgb_to_yuv_f (row[l][0], row[l][1], row[l][2],
				   r[k],	// Y pixel to blend
				   g[k],	// U pixel
				   b[k]);	// V pixel
	    yk = yuv_y;				// user's key color (YUV)
	    uk = yuv_u;
	    vk = yuv_v;
	  }
	  else if (color_work == BlendAlgebraConfig::HSV)
	  {
	    HSV::rgb_to_hsv (row[l][0], row[l][1], row[l][2],
			     r[k],		// H pixel to blend
			     g[k],		// S pixel
			     b[k]);		// V pixel
	    yk = hsv_h;				// user's key color (HSV)
	    uk = hsv_s;
	    vk = hsv_v;
	  }
	  else	// either RGB or by PROJECT, no change
	  {
	    r[k] = row[l][0];			// RGB pixel to blend
	    g[k] = row[l][1];
	    b[k] = row[l][2];
	    yk   = rgb_r;			// user's key color (RGB)
	    uk   = rgb_g;
	    vk   = rgb_b;
	  }	// if color_work
	  a[k] = has_alpha ? row[l][3] : 1;
	  if (config.clear_input)	// leave output track exclusively
	  {
	    row[l][0] = row[l][1] = row[l][2] = 0;
	    if (has_alpha) row[l][3] = 0;
	  }
	  k += step;
	}	// scan tracks for l = 0 .. layers
	out_r = r[arg_out];	// preinitialize args holding output results
	out_g = g[arg_out];
	out_b = b[arg_out];
	out_a = a[arg_out];

	// Call user function
	curr_func.proc (layers, r, g, b, a, yk, uk, vk, key_a,
			&out_r, &out_g, &out_b, &out_a,
			j, i, width, height, has_alpha);

	if (clip_colors) CLAMP (out_a, 0, 1);
	if (color_work == BlendAlgebraConfig::YUV)
	{
	  if (clip_colors)
	  {
	    CLAMP (out_r,  0,   1);
	    CLAMP (out_g, -0.5, 0.5);
	    CLAMP (out_b, -0.5, 0.5);
	  }
	  if (! (isfinite(out_r) && isfinite(out_g) &&
		 isfinite(out_b) && isfinite(out_a)))
	  {	// substitute NaN or unclipped infinity with user's color (YUV)
	    out_r = yuv_y;
	    out_g = yuv_u;
	    out_b = yuv_v;
	    out_a = key_a;
	  }
	  YUV::yuv.yuv_to_rgb_f (row[trk_out][0],	// R
				 row[trk_out][1],	// G
				 row[trk_out][2],	// B
				 out_r,			// Y
				 out_g,			// U
				 out_b);		// V
	}
	else if (color_work == BlendAlgebraConfig::HSV)
	{
	  if (clip_colors)
	  {
	    if (isfinite(out_r) && (out_r < 0 || out_r >= 360))
	      out_r -= floor(out_r/360)*360;	// cannot clamp infinity here
	    CLAMP (out_g, 0, 1);
	    CLAMP (out_b, 0, 1);
	  }
	  if (! (isfinite(out_r) && isfinite(out_g) &&
		 isfinite(out_b) && isfinite(out_a)))
	  {	// substitute NaN or unclipped infinity with user's color (HSV)
	    out_r = hsv_h;
	    out_g = hsv_s;
	    out_b = hsv_v;
	    out_a = key_a;
	  }
	  HSV::hsv_to_rgb (row[trk_out][0], row[trk_out][1], row[trk_out][2],
			   out_r,		// H
			   out_g,		// S
			   out_b);		// V
	}
	else	// either RGB or by PROJECT, no change, clip only
	{
	  if (clip_colors)
	  {
	    CLAMP (out_r, 0, 1);
	    CLAMP (out_g, 0, 1);
	    CLAMP (out_b, 0, 1);
	  }
	  if (! (isfinite(out_r) && isfinite(out_g) &&
		 isfinite(out_b) && isfinite(out_a)))
	  {	// substitute NaN or unclipped infinity with user's color (RGB)
	    out_r = rgb_r;
	    out_g = rgb_g;
	    out_b = rgb_b;
	    out_a = key_a;
	  }
	  row[trk_out][0] = out_r;
	  row[trk_out][1] = out_g;
	  row[trk_out][2] = out_b;
	}	// if color_work
	if (! has_alpha)		// evtl simulate alpha channel
	{
	  row[trk_out][0] *= out_a;
	  row[trk_out][1] *= out_a;
	  row[trk_out][2] *= out_a;
	}				// store real alpha channel
	if (has_alpha) row[trk_out][3] = out_a;
	for (l=0; l<layers; l++)	// increment all rows to next pixel
	{
	  row[l] += 3;
	  if (has_alpha) row[l] ++;
	}
      }		// scan pixels for j = 0 .. width
    }		// scan rows for i = y1 .. y2
    break;

  case BC_RGB888:		// RGB  [ 0 .. 1 ], must be in bounds
  case BC_RGBA8888:		// RGBA [ 0 .. 1 ], must be in bounds
    for (int i=y1; i<y2; i++)		// scan all rows
    {
      unsigned char *row[layers];
      for (l=0; l<layers; l++)
	row[l] = (unsigned char *)frame[l]->get_rows()[i];
      for (int j=0; j<width; j++)	// scan all pixels
      {
	k = start;
	for (l=0; l<layers; l++)	// convert source frames to args
	{
	  if (color_work == BlendAlgebraConfig::YUV)
	  {
	    YUV::yuv.rgb_to_yuv_f ((float)row[l][0]/255,
				   (float)row[l][1]/255,
				   (float)row[l][2]/255,
				   r[k],	// Y pixel to blend
				   g[k],	// U pixel
				   b[k]);	// V pixel
	    yk = yuv_y;				// user's key color (YUV)
	    uk = yuv_u;
	    vk = yuv_v;
	  }
	  else if (color_work == BlendAlgebraConfig::HSV)
	  {
	    HSV::rgb_to_hsv ((float)row[l][0]/255,
			     (float)row[l][1]/255,
			     (float)row[l][2]/255,
			     r[k],		// H pixel to blend
			     g[k],		// S pixel
			     b[k]);		// V pixel
	    yk = hsv_h;				// user's key color (HSV)
	    uk = hsv_s;
	    vk = hsv_v;
	  }
	  else	// either RGB or by PROJECT, conversion to float only
	  {
	    r[k] = (float)row[l][0]/255;	// RGB pixel to blend
	    g[k] = (float)row[l][1]/255;
	    b[k] = (float)row[l][2]/255;
	    yk   = rgb_r;			// user's key color (RGB)
	    uk   = rgb_g;
	    vk   = rgb_b;
	  }	// if color_work
	  a[k] = has_alpha ? (float)row[l][3]/255 : 1;
	  if (config.clear_input)	// leave output track exclusively
	  {
	    row[l][0] = row[l][1] = row[l][2] = 0;
	    if (has_alpha) row[l][3] = 0;
	  }
	  k += step;
	}	// scan tracks for l = 0 .. layers
	out_r = r[arg_out];	// preinitialize args holding output results
	out_g = g[arg_out];
	out_b = b[arg_out];
	out_a = a[arg_out];

	// Call user function
	curr_func.proc (layers, r, g, b, a, yk, uk, vk, key_a,
			&out_r, &out_g, &out_b, &out_a,
			j, i, width, height, has_alpha);

	if (clip_colors) CLAMP (out_a, 0, 1);
	if (color_work == BlendAlgebraConfig::YUV)
	{
	  if (clip_colors)
	  {
	    CLAMP (out_r,  0,   1);
	    CLAMP (out_g, -0.5, 0.5);
	    CLAMP (out_b, -0.5, 0.5);
	  }
	  if (! (isfinite(out_r) && isfinite(out_g) &&
		 isfinite(out_b) && isfinite(out_a)))
	  {	// substitute NaN or unclipped infinity with user's color (YUV)
	    out_r = yuv_y;
	    out_g = yuv_u;
	    out_b = yuv_v;
	    out_a = key_a;
	  }
	  YUV::yuv.yuv_to_rgb_f (rk, gk, bk,
				 out_r,		// Y
				 out_g,		// U
				 out_b);	// V
	}
	else if (color_work == BlendAlgebraConfig::HSV)
	{
	  if (clip_colors)
	  {
	    if (isfinite(out_r) && (out_r < 0 || out_r >= 360))
	      out_r -= floor(out_r/360)*360;	// cannot clamp infinity here
	    CLAMP (out_g, 0, 1);
	    CLAMP (out_b, 0, 1);
	  }
	  if (! (isfinite(out_r) && isfinite(out_g) &&
		 isfinite(out_b) && isfinite(out_a)))
	  {	// substitute NaN or unclipped infinity with user's color (HSV)
	    out_r = hsv_h;
	    out_g = hsv_s;
	    out_b = hsv_v;
	    out_a = key_a;
	  }
	  HSV::hsv_to_rgb (rk, gk, bk,
			   out_r,		// H
			   out_g,		// S
			   out_b);		// V
	}
	else	// either RGB or by PROJECT, no change, clip only
	{
	  if (clip_colors)
	  {
	    CLAMP (out_r, 0, 1);
	    CLAMP (out_g, 0, 1);
	    CLAMP (out_b, 0, 1);
	  }
	  if (! (isfinite(out_r) && isfinite(out_g) &&
		 isfinite(out_b) && isfinite(out_a)))
	  {	// substitute NaN or unclipped infinity with user's color (RGB)
	    out_r = rgb_r;
	    out_g = rgb_g;
	    out_b = rgb_b;
	    out_a = key_a;
	  }
	  rk = out_r;
	  gk = out_g;
	  bk = out_b;
	}	// if color_work
	if (! has_alpha)		// evtl simulate alpha channel
	{
	  rk *= out_a;
	  gk *= out_a;
	  bk *= out_a;
	}
	row[trk_out][0] = (unsigned char) CLIP (rk*255, 0, 255);//reformat/clip
	row[trk_out][1] = (unsigned char) CLIP (gk*255, 0, 255);
	row[trk_out][2] = (unsigned char) CLIP (bk*255, 0, 255);
	if (has_alpha)			// store real alpha channel
	  row[trk_out][3] = (unsigned char) CLIP (out_a*255, 0, 255);
	for (l=0; l<layers; l++)	// increment all rows to next pixel
	{
	  row[l] += 3;
	  if (has_alpha) row[l] ++;
	}
      }		// scan pixels for j = 0 .. width
    }		// scan rows for i = y1 .. y2
    break;

  case BC_YUV888:	// R  [ 0 .. 1 ], GB [ -0.5 .. 0.5 ], must be in bounds
  case BC_YUVA8888:	// RA [ 0 .. 1 ], GB [ -0.5 .. 0.5 ], must be in bounds
    for (int i=y1; i<y2; i++)		// scan all rows
    {
      unsigned char *row[layers];
      for (l=0; l<layers; l++)
	row[l] = (unsigned char *)frame[l]->get_rows()[i];
      for (int j=0; j<width; j++)	// scan all pixels
      {
	k = start;
	for (l=0; l<layers; l++)	// convert source frames to args
	{
	  if (color_work == BlendAlgebraConfig::RGB)
	  {
	    YUV::yuv.yuv_to_rgb_f (r[k], g[k], b[k], // RGB pixel to blend
				   (float)row[l][0]/255,
				   ((float)row[l][1]-128)/256,
				   ((float)row[l][2]-128)/256);
	    yk = rgb_r;				// user's key color (RGB)
	    uk = rgb_g;
	    vk = rgb_b;
	  }
	  else if (color_work == BlendAlgebraConfig::HSV)
	  {
	    YUV::yuv.yuv_to_rgb_f (rk, gk, bk,	// RGB temporary pixel
				   (float)row[l][0]/255,
				   ((float)row[l][1]-128)/256,
				   ((float)row[l][2]-128)/256);
	    HSV::rgb_to_hsv (rk, gk, bk,
			     r[k],		// H pixel to blend
			     g[k],		// S pixel
			     b[k]);		// V pixel
	    yk = hsv_h;				// user's key color (HSV)
	    uk = hsv_s;
	    vk = hsv_v;
	  }
	  else	// either YUV or by PROJECT, conversion to float only
	  {
	    r[k] =  (float)row[l][0]/255;	// Y pixel to blend
	    g[k] = ((float)row[l][1]-128)/256;	// U pixel
	    b[k] = ((float)row[l][2]-128)/256;	// V pixel
	    yk = yuv_y;				// user's key color (YUV)
	    uk = yuv_u;
	    vk = yuv_v;
	  }	// if color_work
	  a[k] = has_alpha ? (float)row[l][3]/255 : 1;
	  if (config.clear_input)	// leave output track exclusively
	  {
	    row[l][0] = row[l][1] = row[l][2] = 0;
	    if (has_alpha) row[l][3] = 0;
	  }
	  k += step;
	}	// scan tracks for l = 0 .. layers
	out_r = r[arg_out];	// preinitialize args holding output results
	out_g = g[arg_out];
	out_b = b[arg_out];
	out_a = a[arg_out];

	// Call user function
	curr_func.proc (layers, r, g, b, a, yk, uk, vk, key_a,
			&out_r, &out_g, &out_b, &out_a,
			j, i, width, height, has_alpha);

	if (clip_colors) CLAMP (out_a, 0, 1);
	if (color_work == BlendAlgebraConfig::RGB)
	{
	  if (clip_colors)
	  {
	    CLAMP (out_r, 0, 1);
	    CLAMP (out_g, 0, 1);
	    CLAMP (out_b, 0, 1);
	  }
	  if (! (isfinite(out_r) && isfinite(out_g) &&
		 isfinite(out_b) && isfinite(out_a)))
	  {	// substitute NaN or unclipped infinity with user's color (RGB)
	    out_r = rgb_r;
	    out_g = rgb_g;
	    out_b = rgb_b;
	    out_a = key_a;
	  }
	  YUV::yuv.rgb_to_yuv_f (out_r, out_g, out_b, yk, uk, vk);
	}
	else if (color_work == BlendAlgebraConfig::HSV)
	{
	  if (clip_colors)
	  {
	    if (isfinite(out_r) && (out_r < 0 || out_r >= 360))
	      out_r -= floor(out_r/360)*360;	// cannot clamp infinity here
	    CLAMP (out_g, 0, 1);
	    CLAMP (out_b, 0, 1);
	  }
	  if (! (isfinite(out_r) && isfinite(out_g) &&
		 isfinite(out_b) && isfinite(out_a)))
	  {	// substitute NaN or unclipped infinity with user's color (HSV)
	    out_r = hsv_h;
	    out_g = hsv_s;
	    out_b = hsv_v;
	    out_a = key_a;
	  }			  //     H      S      V
	  HSV::hsv_to_rgb (rk, gk, bk, out_r, out_g, out_b);
	  if (clip_colors)
	  {
	    CLAMP (out_r, 0, 1);
	    CLAMP (out_g, 0, 1);
	    CLAMP (out_b, 0, 1);
	  }
	  YUV::yuv.rgb_to_yuv_f (rk, gk, bk, yk, uk, vk);
	}
	else	// either YUV or by PROJECT, no change, clip only
	{
	  if (clip_colors)
	  {
	    CLAMP (out_r,  0,   1);
	    CLAMP (out_g, -0.5, 0.5);
	    CLAMP (out_b, -0.5, 0.5);
	  }
	  if (! (isfinite(out_r) && isfinite(out_g) &&
		 isfinite(out_b) && isfinite(out_a)))
	  {	// substitute NaN or unclipped infinity with user's color (YUV)
	    out_r = yuv_y;
	    out_g = yuv_u;
	    out_b = yuv_v;
	    out_a = key_a;
	  }
	  yk = out_r;
	  uk = out_g;
	  vk = out_b;
	}	// if color_work
	if (! has_alpha)		// evtl simulate alpha channel
	{
	  yk *= out_a;
	  uk *= out_a;
	  vk *= out_a;
	}
	row[trk_out][0] = (unsigned char) CLIP (yk*255,       0, 255);
	row[trk_out][1] = (unsigned char) CLIP ((uk+0.5)*256, 0, 255);
	row[trk_out][2] = (unsigned char) CLIP ((vk+0.5)*256, 0, 255);
	if (has_alpha)			// store real alpha channel
	  row[trk_out][3] = (unsigned char) CLIP (out_a*255, 0, 255);
	for (l=0; l<layers; l++)	// increment all rows to next pixel
	{
	  row[l] += 3;
	  if (has_alpha) row[l] ++;
	}
      }		// scan pixels for j = 0 .. width
    }		// scan rows for i = y1 .. y2
    break;

  default:
    break;
  }		// switch color_proj
}						// WHEW !!!

void BlendAlgebra::save_data(KeyFrame *keyframe)
{
  FileXML output;

  output.set_shared_output(keyframe->xbuf);

  output.tag.set_title("BLEND_ALGEBRA");
  output.tag.set_property("FUNCNAME",     config.funcname);
  output.tag.set_property("PARALLEL",     config.parallel);
  output.tag.set_property("DIRECTION",    config.direction);
  output.tag.set_property("OUTPUT_TRACK", config.output_track);
  output.tag.set_property("COLORSPACE",   config.colorspace);
  output.tag.set_property("CLIPCOLORS",   config.clipcolors);
  output.tag.set_property("CLEAR_INPUT",  config.clear_input);
  output.tag.set_property("RED",          config.red);
  output.tag.set_property("GREEN",        config.green);
  output.tag.set_property("BLUE",         config.blue);
  output.tag.set_property("ALPHA",        config.alpha);
  output.append_tag();
  output.tag.set_title("/BLEND_ALGEBRA");
  output.append_tag();
  output.append_newline();
  output.terminate_string();
}

void BlendAlgebra::read_data(KeyFrame *keyframe)
{
  FileXML input;

  input.set_shared_input(keyframe->xbuf);

  while(!input.read_tag())
  {
    if(input.tag.title_is("BLEND_ALGEBRA"))
    {
      input.tag.get_property("FUNCNAME", config.funcname);
      config.parallel  = input.tag.get_property("PARALLEL",  config.parallel);
      config.direction = input.tag.get_property("DIRECTION", config.direction);
      config.output_track =
	input.tag.get_property("OUTPUT_TRACK", config.output_track);
      config.colorspace =
	input.tag.get_property("COLORSPACE", config.colorspace);
      config.clipcolors =
	input.tag.get_property("CLIPCOLORS", config.clipcolors);
      config.clear_input =
	input.tag.get_property("CLEAR_INPUT", config.clear_input);
      config.red   = input.tag.get_property("RED",   config.red);
      config.green = input.tag.get_property("GREEN", config.green);
      config.blue  = input.tag.get_property("BLUE",  config.blue);
      config.alpha = input.tag.get_property("ALPHA", config.alpha);
    }
  }
}

void BlendAlgebra::update_gui()
{
  if( ! thread ) return;
  if( ! (load_configuration() || inspect_configuration) ) return;
  inspect_configuration = 0;	// update once after change or after creation
  thread->window->lock_window("BlendAlgebra::update_gui");
  BlendAlgebraWindow *window = (BlendAlgebraWindow*)thread->window;

  window->funcname->update(config.funcname);
  window->parallel->update(config.parallel);
  window->clipcolors->update(config.clipcolors);
  window->clear_input->update(config.clear_input);
  window->direction->set_text(
    BlendAlgebraConfig::direction_to_text(config.direction));
  window->output->set_text(
    BlendAlgebraConfig::output_to_text(config.output_track));
  window->colorspace->set_text(
    BlendAlgebraConfig::colorspace_to_text(config.colorspace));
  window->update_key_sample();
  window->alpha_text->update(config.alpha);
  window->key_alpha->update(config.alpha);

  thread->window->unlock_window();
}

////////////////////////////////////////////
// Multithreaded processing stuff
////////////////////////////////////////////

BlendAlgebraEngine::BlendAlgebraEngine(BlendAlgebra *plugin, 
				       int total_clients, 
				       int total_packages)
  : LoadServer(total_clients, total_packages)
{
  this->plugin = plugin;
}

void BlendAlgebraEngine::init_packages ()
{
  for (int i=0; i<get_total_packages(); i++)
  {
    BlendAlgebraPackage *pkg = (BlendAlgebraPackage *) get_package (i);
    pkg->y1 = plugin->height *  i      / get_total_packages ();
    pkg->y2 = plugin->height * (i + 1) / get_total_packages ();
  }
}

LoadClient *BlendAlgebraEngine::new_client ()
{
  return new BlendAlgebraUnit (plugin, this);
}

LoadPackage *BlendAlgebraEngine::new_package ()
{
  return new BlendAlgebraPackage;
}

BlendAlgebraPackage::BlendAlgebraPackage()
  : LoadPackage()
{
}

BlendAlgebraUnit::BlendAlgebraUnit (BlendAlgebra *plugin,
				    BlendAlgebraEngine *engine)
  : LoadClient (engine)
{
  this->plugin = plugin;
  this->engine = engine;
}

void BlendAlgebraUnit::process_package(LoadPackage *package)
{
  BlendAlgebraPackage *pkg = (BlendAlgebraPackage *) package;

  plugin->process_frames (pkg->y1, pkg->y2);
}
