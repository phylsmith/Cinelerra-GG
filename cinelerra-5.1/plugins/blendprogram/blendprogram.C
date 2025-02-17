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
#include "blendprogram.h"

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
static time_t BlendProgramTstamp = -1;

REGISTER_PLUGIN(BlendProgram)

LOAD_CONFIGURATION_MACRO(BlendProgram, BlendProgramConfig)

NEW_WINDOW_MACRO(BlendProgram, BlendProgramWindow)

const char *BlendProgram::plugin_title() { return N_("Blend Program"); }

int BlendProgram::is_realtime()     { return 1; }
int BlendProgram::is_multichannel() { return 1; }
int BlendProgram::is_synthesis()    { return 1; }

////////////////////////////////////////////
// Plugin configuration class implementation
////////////////////////////////////////////

BlendProgramConfig::BlendProgramConfig()
{
  funcname[0] = 0;				// no function per default
  parallel    = 1;				// parallelize per default
  clipcolors  = 1;				// clip colors per default
  direction   = BlendProgramConfig::BOTTOM_FIRST; // as in Overlay plugin
  colorspace  = BlendProgramConfig::AUTO;	// requested from function
  red = green = blue = 0;			// black key color per default
  alpha = 0;					// transparent per default
}

int BlendProgramConfig::equivalent(BlendProgramConfig &that)
{
  return
    !strcmp (funcname, that.funcname) &&
    parallel   == that.parallel       &&
    clipcolors == that.clipcolors     &&
    direction  == that.direction      &&
    colorspace == that.colorspace     &&
    EQUIV (red,   that.red)           &&
    EQUIV (green, that.green)         &&
    EQUIV (blue,  that.blue)          &&
    EQUIV (alpha, that.alpha);
}

void BlendProgramConfig::copy_from(BlendProgramConfig &that)
{
  strcpy (funcname, that.funcname);
  parallel   = that.parallel;
  clipcolors = that.clipcolors;
  direction  = that.direction;
  colorspace = that.colorspace;
  red        = that.red;
  green      = that.green;
  blue       = that.blue;
  alpha      = that.alpha;
}

void BlendProgramConfig::interpolate (BlendProgramConfig &prev,
				      BlendProgramConfig &next,
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
  parallel   = prev.parallel;
  clipcolors = prev.clipcolors;
  direction  = prev.direction;
  colorspace = prev.colorspace;
}

const char *BlendProgramConfig::direction_to_text(int direction)
{
  switch(direction)
  {
  case BlendProgramConfig::BOTTOM_FIRST: return _("Bottom first");
  case BlendProgramConfig::TOP_FIRST:    return _("Top first");
  }
  return "";
}

const char *BlendProgramConfig::colorspace_to_text(int colorspace)
{
  switch(colorspace)
  {
  case BlendProgramConfig::AUTO:    return _("auto");
  case BlendProgramConfig::RGB:     return _("RGB");
  case BlendProgramConfig::YUV:     return _("YUV");
  case BlendProgramConfig::HSV:     return _("HSV");
  case BlendProgramConfig::PROJECT: return _("of project");
  }
  return "";
}

int BlendProgramConfig::get_key_color()
{
  int red   = (int) (CLIP (this->red,   0, 1) * 255);
  int green = (int) (CLIP (this->green, 0, 1) * 255);
  int blue  = (int) (CLIP (this->blue,  0, 1) * 255);
  return (red << 16) | (green << 8) | blue;
}

////////////////////////////////////////////
// Plugin dialog window class implementation
////////////////////////////////////////////

BlendProgramFuncname::BlendProgramFuncname(BlendProgram *plugin,
					   const char *funcname,
					   BlendProgramWindow *gui,
					   int x, int y)
  : BC_TextBox(x, y, gui->get_w()-x-xS(10), 1, funcname)
{
  this->plugin = plugin;
  this->gui = gui;
}

int BlendProgramFuncname::handle_event()
{
  // Perhaps locking is not needed here
  // as GUI is driven by a separate plugin instance
  plugin->func_lock->lock("BlendProgramFuncname::handle_event");
  strncpy(plugin->config.funcname, get_text(),
	  sizeof(plugin->config.funcname)-1);
  BlendProgramTstamp = time(NULL);	// time of possible function change
#ifdef DEBUG
  printf ("BlendProgramFuncname::handle_event setting function %s\n   timestamp %s",
	  plugin->config.funcname, ctime(&BlendProgramTstamp));
#endif
  plugin->func_lock->unlock();
  plugin->send_configure_change();
  return 1;
}

BlendProgramDetach::BlendProgramDetach (BlendProgram *plugin,
					BlendProgramWindow *gui,
					int x, int y)
  : BC_GenericButton (x, y, _("Detach"))
{
  this->plugin = plugin;
  this->gui = gui;
}

int BlendProgramDetach::handle_event()
{
  if (! plugin->config.funcname[0]) return 1;// already detached, nothing to do

  plugin->func_lock->lock("BlendProgramDetach::handle_event");
  plugin->config.funcname[0] = 0;	// clear function, inducing detach
  BlendProgramTstamp = time(NULL);	// force refresh of dlopen'd functions
#ifdef DEBUG
  printf ("BlendProgramDetach::handle_event clearing function\n   timestamp %s",
	  ctime(&BlendProgramTstamp));
#endif
  plugin->func_lock->unlock();

  gui->lock_window("BlendProgramDetach::handle_event");
  gui->funcname->update(plugin->config.funcname);
  gui->unlock_window();

  plugin->send_configure_change();
  return 1;
}

BlendProgramRefresh::BlendProgramRefresh (BlendProgram *plugin,
					  BlendProgramWindow *gui,
					  int x, int y)
  : BC_GenericButton (x, y, _("Refresh"))
{
  this->plugin = plugin;
  this->gui = gui;
}

int BlendProgramRefresh::handle_event()
{
  plugin->func_lock->lock("BlendProgramRefresh::handle_event");
  BlendProgramTstamp = time(NULL);	// force refresh of dlopen'd functions
#ifdef DEBUG
  printf ("BlendProgramRefresh::handle_event timestamp %s",
	  ctime(&BlendProgramTstamp));
#endif
  plugin->func_lock->unlock();
  return 1;	// just reattach all functions, without reconfiguration
}

BlendProgramEdit::BlendProgramEdit (BlendProgram *plugin,
				    BlendProgramWindow *gui,
				    int x, int y)
  : BC_GenericButton (x, y, _("Edit..."))
{
  this->plugin = plugin;
  this->gui = gui;
}

int BlendProgramEdit::handle_event()
{
  char fname[BCTEXTLEN], dir[BCTEXTLEN], str[2*BCTEXTLEN];

  strcpy (fname, plugin->config.funcname);
  if (! fname[0])
  {
    eprintf (_("Blend Program: no source file to edit, select program first\n"));
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
  sprintf(str, "\"%s/dlfcn/BlendProgramCompile.pl\" -edit \"%s\"",
	  getenv("CIN_DAT"), fname);
#ifdef DEBUG
  printf ("BlendProgramEdit::handle_event: executing:\n   %s\n", str);
#endif
  system (str);				// runs configured external editor

  plugin->func_lock->lock("BlendProgramEdit::handle_event");
  BlendProgramTstamp = time(NULL);	// force refresh of dlopen'd functions
#ifdef DEBUG
  printf ("BlendProgramEdit::handle_event edited function %s\n   timestamp %s",
	  fname, ctime(&BlendProgramTstamp));
#endif
  plugin->func_lock->unlock();

  // Evtl functions will be recompiled, but no configure change
  return 1;
}

BlendProgramFileButton::BlendProgramFileButton(BlendProgram *plugin,
					       BlendProgramWindow *gui,
					       int x, int y)
  : BC_GenericButton(x, y, _("Attach..."))
{
  this->plugin = plugin;
  this->gui = gui;
  this->file_box = 0;
}

BlendProgramFileButton::~BlendProgramFileButton()
{
  stop();
}

int BlendProgramFileButton::handle_event()
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

void BlendProgramFileButton::run()
{
  int result = 1;
  const char *fpath;
  char fname[BCTEXTLEN], dir[BCTEXTLEN];

  strcpy (fname, plugin->config.funcname);

  // This infinite loop is exited after clicking OK or Cancel in FileBox.
  // There are several special buttons which replace the FileBox initial path
  // with another predefined path and close FileBox with reinit_path flag set.
  // If reinit_path is set, the loop is repeated with that extracted path.
  // reinit_path is cleared inside BlendProgramFileBox constructor.

  for (;;)		// will exit when reinit_path == 0
  {	// Evtl make function path absolute by prepending project path to it
    if (fname[0] != '/') // fname is relative, prepend current project path
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
#ifdef DEBUG
    printf ("BlendProgramFileButton::run creating file_box (%s)\n", fname);
#endif
    file_box = new BlendProgramFileBox (plugin, gui, fname);
    file_box->update_history();	// otherwise actual dir can be forgotten
    file_box->create_objects();
    file_box->lock_window ("BlendProgramFileButton::run");
    file_box->add_objects();			// add our special buttons
    file_box->update_filter ("*.bp");
    file_box->unlock_window();
    result = file_box->run_window();
    if (file_box->reinit_path)			// if set, a button was clicked
    {
      fpath = file_box->get_current_path();	// current, as set by buttons
#ifdef DEBUG
      printf ("BlendProgramFileButton::run file_box returned %d reinit_path=%d\n   fpath=%s\n",
	      result, file_box->reinit_path, fpath);
#endif
      strncpy (fname, fpath ? fpath : "", sizeof(fname)-1);
      delete file_box;
      file_box = 0;
      continue;	// reinit_path will be cleared on repeat in FileBox constructor
    }
    fpath = file_box->get_submitted_path();	// submitted, as set by user
#ifdef DEBUG
    printf ("BlendProgramFileButton::run file_box returned %d reinit_path=%d\n   fpath=%s\n",
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
      char *cp = strrchr (dir, '/');
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
    if (strlen (fname) < 3 || strcmp (fname+strlen(fname)-3, ".bp"))
      strcat (fname, ".bp");	// suggest '.bp' suffix for blend programs
  }

  // Actualize selected function in config and in the main plugin dialog
  plugin->func_lock->lock("BlendProgramFileButton::run");
  strcpy (plugin->config.funcname, fname);
  BlendProgramTstamp = time(NULL);	// time of possible function change
#ifdef DEBUG
  printf ("BlendProgramFileButton::run setting function %s\n   timestamp %s",
	  plugin->config.funcname, ctime(&BlendProgramTstamp));
#endif
  plugin->func_lock->unlock();
  gui->lock_window("BlendProgramFileButton::run");
  gui->funcname->update(plugin->config.funcname);
  gui->unlock_window();
  gui->editing_lock->lock();
  gui->editing = 0;
  gui->editing_lock->unlock();

  plugin->send_configure_change();
}

void BlendProgramFileButton::stop()
{
  if (file_box) file_box->set_done(1);
  join();
}

BlendProgramFileBox::BlendProgramFileBox(BlendProgram *plugin,
					 BlendProgramWindow *gui,
					 char *init_path)
  : BC_FileBox(0, BC_WindowBase::get_resources()->filebox_h/2, init_path,
	       _("Blend Program: Select program source file"),"")
{
  this->plugin = plugin;
  this->gui = gui;

  to_curdir   = 0;
  to_usrlib   = 0;
  to_syslib   = 0;
  copy_curdir = 0;
  copy_usrlib = 0;
  file_edit   = 0;
  reinit_path = 0;
}

BlendProgramFileBox::~BlendProgramFileBox()
{
}

// We need several additional buttons not foreseen in the bare FileBox.
// We arrange them in the place of (empty) FileBox caption.
void BlendProgramFileBox::add_objects()
{
  int xs10 = xS(10), xs5 = xS(5);
  int ys10 = yS(10);
  int x = xs10, y = ys10, x2;

  add_subwindow(to_curdir = new BlendProgramToCurdir(this, x, y));
  x2 = x+to_curdir->get_w()+xs5;
  add_subwindow(to_usrlib = new BlendProgramToUsrlib(this, x2, y));
  x2 += to_usrlib->get_w()+xs5;
  add_subwindow(to_syslib = new BlendProgramToSyslib(this, x2, y));
  y = get_y_margin();
  add_subwindow(copy_curdir = new BlendProgramCopyCurdir(this, x, y));
  x2 = x+copy_curdir->get_w()+xs5;
  add_subwindow(copy_usrlib = new BlendProgramCopyUsrlib(this, x2, y));
  x2 += copy_usrlib->get_w()+xs5;
  add_subwindow(file_edit = new BlendProgramFileEdit(this, x2, y));
  flush();
}

int BlendProgramFileBox::resize_event(int w, int h)
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

BlendProgramToCurdir::BlendProgramToCurdir(BlendProgramFileBox *file_box,
					   int x, int y)
  : BC_GenericButton (x, y, _("=>Project"))
{
  this->file_box = file_box;
}

int BlendProgramToCurdir::handle_event()
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
  else strcpy (path, dir);	// substitute old entered dir with project dir

  // Not exactly sure what operations on FileBox are really important
  file_box->fs->change_dir (dir);	// force it to recognize the new dir

  // This updates all paths, sets current_path and submitted_path of FileBox,
  // but in memory only, text fields in the dialog are not actualized.
  // file_box->refresh() does not help to refresh text fields either.
  // Therefore we have to apply a trick with closing FileBox and
  // reopening it with the new generated path.
  file_box->update_paths (path);

  // Without updating history FileBox forgets our new dir
  // and sets curdir to some old history item.
  file_box->update_history();

  file_box->reinit_path = 1;	// set flag to reopen FileBox afterwards
  file_box->set_done(1); // temporarily close FileBox, will be reopened later

  return 1;
}

BlendProgramToUsrlib::BlendProgramToUsrlib(BlendProgramFileBox *file_box,
					   int x, int y)
  : BC_GenericButton (x, y, _("=>Userlib"))
{
  this->file_box = file_box;
}

int BlendProgramToUsrlib::handle_event()
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

  // The default user libdir for blend programs is $HOME/.bcast5lib/dlfcn/bp
  // Ensure it is a directory, evtl create dir, if not - do nothing else
  if (! file_box->fs->is_dir (dir)) file_box->fs->create_dir (dir);
  if (! file_box->fs->is_dir (dir)) return 1;
  strcat (dir, "/dlfcn");
  if (! file_box->fs->is_dir (dir)) file_box->fs->create_dir (dir);
  if (! file_box->fs->is_dir (dir)) return 1;
  strcat (dir, "/bp");
  if (! file_box->fs->is_dir (dir)) file_box->fs->create_dir (dir);
  if (! file_box->fs->is_dir (dir)) return 1;

  fname[0] = 0;
  const char *spath = file_box->get_submitted_path();// get name entered so far
  if (spath) file_box->fs->extract_name (fname, spath);	// cut name from dir
  if (fname[0]) file_box->fs->join_names (path, dir, fname);
  else strcpy (path, dir);	// substitute old entered dir with user libdir

  // Reinitialize FileBox with the modified path
  file_box->fs->change_dir (dir);
  file_box->update_paths (path);
  file_box->update_history();
  file_box->reinit_path = 1;	// set flag to reopen FileBox afterwards
  file_box->set_done(1); // temporarily close FileBox, will be reopened later

  return 1;
}

BlendProgramToSyslib::BlendProgramToSyslib(BlendProgramFileBox *file_box,
					   int x, int y)
  : BC_GenericButton (x, y, _("=>Syslib"))
{
  this->file_box = file_box;
}

int BlendProgramToSyslib::handle_event()
{
  char *cp, fname[BCTEXTLEN], dir[BCTEXTLEN], path[BCTEXTLEN];

  dir[0] = 0;
  cp = getenv ("CIN_DAT");	// Cinelerra installation directory (bin)
  if (cp) strcpy (dir, cp);
  if (! dir[0]) return 1;	// there is no default

  // System libdir for blend programs is $CIN_DAT/dlfcn/bp (bin/dlfcn/bp).
  // Ensure it is a directory, it must exist, if not - do nothing else
  strcat (dir, "/dlfcn/bp");
  if (! file_box->fs->is_dir (dir)) return 1;

  fname[0] = 0;
  const char *spath = file_box->get_submitted_path();// get name entered so far
  if (spath) file_box->fs->extract_name (fname, spath);	// cut name from dir
  if (fname[0]) file_box->fs->join_names (path, dir, fname);
  else strcpy (path, dir);	// substitute that old dir with system libdir

  // Reinitialize FileBox with the modified path
  file_box->fs->change_dir (dir);
  file_box->update_paths (path);
  file_box->update_history();
  file_box->reinit_path = 1;	// set flag to reopen FileBox afterwards
  file_box->set_done(1); // temporarily close FileBox, will be reopened later

  return 1;
}

BlendProgramCopyCurdir::BlendProgramCopyCurdir(BlendProgramFileBox *file_box,
					       int x, int y)
  : BC_GenericButton (x, y, _("Copy to project"))
{
  this->file_box = file_box;
}

int BlendProgramCopyCurdir::handle_event()
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
    eprintf (_("Blend Program: source file %s does not exist or not readable\n"),
	     from_path);
    return 1;
  }
  if (! access (to_path, F_OK))
  {
    eprintf (_("Blend Program: target file %s exists, overwriting not allowed\n"),
	     to_path);
    return 1;
  }

  // Now do copy operation
  sprintf (cmd, "cp \"%s\" \"%s\"", from_path, to_path);
#ifdef DEBUG
  printf ("BlendProgramCopyCurdir::handle_event: executing %s\n", cmd);
#endif
  ret = system (cmd);
  if (ret)
  {
    eprintf (_("Blend Program: copying %s to %s failed\nsee console printout for diagnostics\n"),
	     from_path, to_path);
    return 1;
  }

  // Copying successful, now change dir to the location of the target
  file_box->fs->change_dir (dir);
  file_box->update_paths (to_path);
  file_box->update_history();
  file_box->reinit_path = 1;	// set flag to reopen FileBox afterwards
  file_box->set_done(1); // temporarily close FileBox, will be reopened later

  return 1;
}

BlendProgramCopyUsrlib::BlendProgramCopyUsrlib(BlendProgramFileBox *file_box,
					       int x, int y)
  : BC_GenericButton (x, y, _("Copy to userlib"))
{
  this->file_box = file_box;
}

int BlendProgramCopyUsrlib::handle_event()
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
  strcat (dir, "/bp");
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
    eprintf (_("Blend Program: source file %s does not exist or not readable\n"),
	     from_path);
    return 1;
  }
  if (! access (to_path, F_OK))
  {
    eprintf (_("Blend Program: target file %s exists, overwriting not allowed\n"),
	     to_path);
    return 1;
  }

  // Now do copy operation
  sprintf (cmd, "cp \"%s\" \"%s\"", from_path, to_path);
#ifdef DEBUG
  printf ("BlendProgramCopyUsrlib::handle_event: executing %s\n", cmd);
#endif
  ret = system (cmd);
  if (ret)
  {
    eprintf (_("Blend Program: copying %s to %s failed\nsee console printout for diagnostics\n"),
	     from_path, to_path);
    return 1;
  }

  return 1; // Copying successful, but don't change directory to user libdir
}

BlendProgramFileEdit::BlendProgramFileEdit(BlendProgramFileBox *file_box,
					   int x, int y)
  : BC_GenericButton (x, y, _("Edit..."))
{
  this->file_box = file_box;
}

int BlendProgramFileEdit::handle_event()
{
  char fname[BCTEXTLEN], dir[BCTEXTLEN], str[2*BCTEXTLEN];

  fname[0] = 0;
  const char *spath = file_box->get_submitted_path();// get name entered so far
  if (spath) strcpy (fname, spath);
  if (! fname[0])
  {
    eprintf (_("Blend Program: no program to edit, select source file first\n"));
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
    eprintf (_("Blend Program: cannot edit directory, select source file first\n"));
    return 1;
  }

  // This will run configured external editor via perl script
  // If editor start is not backgrounded, GUI will block until editor exits
  sprintf(str, "\"%s/dlfcn/BlendProgramCompile.pl\" -edit \"%s\"",
	  getenv("CIN_DAT"), fname);
#ifdef DEBUG
  printf ("BlendProgramFileEdit::handle_event: executing:\n   %s\n", str);
#endif
  system (str);				// runs configured external editor

  file_box->plugin->func_lock->lock("BlendProgramFileEdit::handle_event");
  BlendProgramTstamp = time(NULL);	// force refresh of dlopen'd functions
#ifdef DEBUG
  printf ("BlendProgramFileEdit::handle_event edited function %s\n   timestamp %s",
	  fname, ctime(&BlendProgramTstamp));
#endif
  file_box->plugin->func_lock->unlock();

  return 1;
}

BlendProgramClipcolors::BlendProgramClipcolors(BlendProgram *plugin,
					       BlendProgramWindow *gui,
					       int x, int y)
  : BC_CheckBox(x, y, plugin->config.clipcolors)
{
  this->plugin = plugin;
  this->gui = gui;
}

int BlendProgramClipcolors::handle_event()
{
  plugin->config.clipcolors = get_value();
  plugin->send_configure_change();
  return 1;
}

BlendProgramParallel::BlendProgramParallel(BlendProgram *plugin,
					   BlendProgramWindow *gui,
					   int x, int y)
  : BC_CheckBox(x, y, plugin->config.parallel)
{
  this->plugin = plugin;
  this->gui = gui;
}

int BlendProgramParallel::handle_event()
{
  plugin->config.parallel = get_value();
  plugin->send_configure_change();
  return 1;
}

BlendProgramDirection::BlendProgramDirection(BlendProgram *plugin, int x, int y)
  : BC_PopupMenu(x, y, xS(150),
	BlendProgramConfig::direction_to_text(plugin->config.direction), 1)
{
  this->plugin = plugin;
}

void BlendProgramDirection::create_objects()
{
  add_item(new BC_MenuItem(BlendProgramConfig::direction_to_text(
			     BlendProgramConfig::TOP_FIRST)));
  add_item(new BC_MenuItem(BlendProgramConfig::direction_to_text(
			     BlendProgramConfig::BOTTOM_FIRST)));
}

int BlendProgramDirection::handle_event()
{
  char *text = get_text();

  if(!strcmp(text, BlendProgramConfig::direction_to_text(
	       BlendProgramConfig::TOP_FIRST)))
    plugin->config.direction = BlendProgramConfig::TOP_FIRST;
  else if(!strcmp(text, BlendProgramConfig::direction_to_text(
		    BlendProgramConfig::BOTTOM_FIRST)))
    plugin->config.direction = BlendProgramConfig::BOTTOM_FIRST;

  plugin->send_configure_change();
  return 1;
}

BlendProgramColorspace::BlendProgramColorspace(BlendProgram *plugin,
					       int x, int y)
  : BC_PopupMenu(x, y, xS(150),
	BlendProgramConfig::colorspace_to_text(plugin->config.colorspace), 1)
{
  this->plugin = plugin;
}

void BlendProgramColorspace::create_objects()
{
  add_item(new BC_MenuItem(BlendProgramConfig::colorspace_to_text(
			     BlendProgramConfig::AUTO)));
  add_item(new BC_MenuItem(BlendProgramConfig::colorspace_to_text(
			     BlendProgramConfig::RGB)));
  add_item(new BC_MenuItem(BlendProgramConfig::colorspace_to_text(
			     BlendProgramConfig::YUV)));
  add_item(new BC_MenuItem(BlendProgramConfig::colorspace_to_text(
			     BlendProgramConfig::HSV)));
  add_item(new BC_MenuItem(BlendProgramConfig::colorspace_to_text(
			     BlendProgramConfig::PROJECT)));
}

int BlendProgramColorspace::handle_event()
{
  char *text = get_text();

  if(!strcmp(text, BlendProgramConfig::colorspace_to_text(
	       BlendProgramConfig::AUTO)))
    plugin->config.colorspace = BlendProgramConfig::AUTO;
  else if(!strcmp(text, BlendProgramConfig::colorspace_to_text(
		    BlendProgramConfig::RGB)))
    plugin->config.colorspace = BlendProgramConfig::RGB;
  else if(!strcmp(text, BlendProgramConfig::colorspace_to_text(
		    BlendProgramConfig::YUV)))
    plugin->config.colorspace = BlendProgramConfig::YUV;
  else if(!strcmp(text, BlendProgramConfig::colorspace_to_text(
		    BlendProgramConfig::HSV)))
    plugin->config.colorspace = BlendProgramConfig::HSV;
  else if(!strcmp(text, BlendProgramConfig::colorspace_to_text(
		    BlendProgramConfig::PROJECT)))
    plugin->config.colorspace = BlendProgramConfig::PROJECT;

  plugin->send_configure_change();
  return 1;
}

BlendProgramKeyColor::BlendProgramKeyColor (BlendProgram *plugin,
					    BlendProgramWindow *gui,
					    int x, int y)
  : BC_GenericButton (x, y, _("Select key color..."))
{
  this->plugin = plugin;
  this->gui = gui;
}

int BlendProgramKeyColor::handle_event()
{
  gui->color_thread->start_window (plugin->config.get_key_color(), 0xff);
  return 1;
}

BlendProgramColorPicker::BlendProgramColorPicker (BlendProgram *plugin,
						  BlendProgramWindow *gui,
						  int x, int y)
  : BC_GenericButton (x, y, _("Get from color picker"))
{
  this->plugin = plugin;
  this->gui = gui;
}

int BlendProgramColorPicker::handle_event()
{
  plugin->config.red   = plugin->get_red();
  plugin->config.green = plugin->get_green();
  plugin->config.blue  = plugin->get_blue();

  gui->update_key_sample();

  plugin->send_configure_change();
  return 1;
}

BlendProgramColorThread::BlendProgramColorThread (BlendProgram * plugin,
						  BlendProgramWindow * gui)
  : ColorPicker (0, _("Select color"))
{
  this->plugin = plugin;
  this->gui = gui;
}

int BlendProgramColorThread::handle_new_color (int output, int alpha)
{
  plugin->config.red   = (float) ((output & 0xff0000) >> 16) / 255;
  plugin->config.green = (float) ((output & 0x00ff00) >>  8) / 255;
  plugin->config.blue  = (float) ((output & 0x0000ff)      ) / 255;

  get_gui()->unlock_window();
  gui->lock_window("BlendProgramColorThread::handle_new_color");
  gui->update_key_sample();
  gui->unlock_window();
  get_gui()->lock_window("BlendProgramColorThread::handle_new_color");

  plugin->send_configure_change();
  return 1;
}

BlendProgramAlphaText::BlendProgramAlphaText(BlendProgram *plugin,
					     BlendProgramWindow *gui,
					     BlendProgramAlphaSlider *slider,
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

BlendProgramAlphaText::~BlendProgramAlphaText()
{
}

int BlendProgramAlphaText::handle_event()
{
  *output = atof(get_text());
  if(*output > max) *output = max;
  if(*output < min) *output = min;
  slider->update(*output);

  plugin->send_configure_change();
  return 1;
}

BlendProgramAlphaSlider::BlendProgramAlphaSlider(BlendProgram *plugin,
						 BlendProgramAlphaText *text,
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

BlendProgramAlphaSlider::~BlendProgramAlphaSlider()
{
}

int BlendProgramAlphaSlider::handle_event()
{
  *output = get_value();
  text->update(*output);

  plugin->send_configure_change();
  return 1;
}

BlendProgramWindow::BlendProgramWindow(BlendProgram *plugin)
  : PluginClientWindow(plugin, xS(450), yS(380), xS(450), yS(380), 0)
{
  this->plugin = plugin;
  color_thread = 0;
  editing_lock = new Mutex("BlendProgramWindow::editing_lock");
  editing = 0;
}

BlendProgramWindow::~BlendProgramWindow()
{
  delete color_thread;
  delete editing_lock;
}

void BlendProgramWindow::create_objects()
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
  add_subwindow(title = new BC_Title(x, y, _("Program:")));
  add_subwindow(funcname =
		new BlendProgramFuncname(plugin, plugin->config.funcname, this,
					 x + title->get_w() + xs5, y));

  y += ys30;
  add_subwindow(file_button = new BlendProgramFileButton(plugin, this, x, y));
  x2 = x+file_button->get_w()+xs5;
  add_subwindow(edit_button = new BlendProgramEdit(plugin, this, x2, y));
  x2 += edit_button->get_w()+xs5;
  add_subwindow(refresh_button = new BlendProgramRefresh(plugin, this, x2, y));
  x2 += refresh_button->get_w()+xs5;
  add_subwindow(detach_button = new BlendProgramDetach(plugin, this, x2, y));

  y += ys30;
  add_subwindow(title = new BC_Title(x, y, _("Color space:")));
  add_subwindow(colorspace =
		new BlendProgramColorspace(plugin,
					   x + title->get_w() + xs5, y));
  colorspace->create_objects();

  x2 = x+title->get_w()+colorspace->get_w()+xs10+xs10;
  add_subwindow(title = new BC_Title(x2, y, _("Parallelize processing")));
  add_subwindow(parallel =
		new BlendProgramParallel(plugin, this,
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
		new BlendProgramClipcolors(plugin, this,
					   x + title->get_w() + xs5, y));

  y += ys30+ys5;
  add_subwindow(key_color = new BlendProgramKeyColor(plugin, this, x, y));
  x2 = x+key_color->get_w()+xs5;
  add_subwindow(color_picker =
		new BlendProgramColorPicker(plugin, this, x2, y));

  y += ys30+ys5;
  add_subwindow(title = new BC_Title(x, y, _("Substitution opacity:")));
  alpha_text = new BlendProgramAlphaText (plugin, this, 0,
					  x+title->get_w()+xs10+xs5+xS(210),
					  y, 0, 1, &plugin->config.alpha);
  alpha_text->create_objects();
  key_alpha = new BlendProgramAlphaSlider (plugin, alpha_text,
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
		new BlendProgramDirection(plugin,
					  x + title->get_w() + xs5, y));
  direction->create_objects();

  color_thread = new BlendProgramColorThread(plugin, this);

  update_key_sample();
  show_window();
  flush();
}

void BlendProgramWindow::update_key_sample()
{
  key_sample->set_color (plugin->config.get_key_color());
  key_sample->draw_box (0, 0, key_sample->get_w(), key_sample->get_h());
  key_sample->set_color (BLACK);
  key_sample->draw_rectangle (0, 0, key_sample->get_w(), key_sample->get_h());
  key_sample->flash ();
}

void BlendProgramWindow::done_event()
{
  color_thread->close_window();
}

int BlendProgramWindow::close_event()
{
  color_thread->close_window();
  file_button->stop();
  set_done(1);
  return 1;
}

int BlendProgramWindow::hide_window (int flush)
{
  color_thread->close_window();
  file_button->stop();
  return BC_WindowBase::hide_window (flush);
}

////////////////////////////////////////////
// Plugin main class implementation
////////////////////////////////////////////

BlendProgramFunc::BlendProgramFunc()
{
  src[0] =  0;
  handle =  0;
  proc   =  0;
  init   =  0;
  tstamp = -1;
}

BlendProgramFunc::~BlendProgramFunc()
{
  if (handle)
  {
#ifdef DEBUG
    printf ("BlendProgramFunc destructor detaching function dlclose(%s)\n",
	    src);
#endif
    dlclose (handle);
  }
}

BlendProgram::BlendProgram(PluginServer *server)
  : PluginVClient(server)
{
  BlendProgramTstamp = time(NULL);
  inspect_configuration = 1;			// force initial configuration
  curr_func_no = -1;
  func_lock = new Mutex("BlendProgram::func_lock");
  engine = 0;
#ifdef DEBUG
  printf ("BlendProgram constructor timestamp %s", ctime(&BlendProgramTstamp));
#endif
}

BlendProgram::~BlendProgram()
{
  if (engine) delete engine;
  delete func_lock;
#ifdef DEBUG
  printf ("BlendProgram destructor removing all %d functions\n",
	  funclist.total);
#endif
  funclist.remove_all_objects();
  curr_func.handle = 0;
}

int BlendProgram::process_buffer(VFrame **frame,
				 int64_t start_position,
				 double frame_rate)
{
  BlendProgramFunc *ptr;
  int refresh_eprintf = 0;

  // Mocking up with function shared object if it might get modified
  // Not sure if plugin locking is needed for this separate processing instance
  func_lock->lock("BlendProgram::process_buffer");

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
	printf ("BlendProgram::process_buffer searching function %s out of %d\n",
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
	    printf ("BlendProgram::process_buffer cached function %s found: %d\n   timestamp %s",
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
    if (curr_func.tstamp == -1 || BlendProgramTstamp > curr_func.tstamp)
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
	sprintf(str, "\"%s/dlfcn/BlendProgramCompile.pl\" \"%s\"",
		getenv("CIN_DAT"), path);
#ifdef DEBUG
	printf ("BlendProgram::process_buffer\n   curr_func.tstamp %s",
		ctime(&curr_func.tstamp));
	printf ("   global tstamp %s   executing %s\n",
		ctime(&BlendProgramTstamp), str);
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
	printf ("BlendProgram::process_buffer cannot access function %s\n",
		curr_func.src);
#endif
      }

      // Now test if function relinking needed, make function cache consistent
      if (tstamp == -1)
      {		// either function does not exist or compilation unsuccessful
	if (fd > -1)
	  eprintf (_("Blend Program: compilation of program %s failed\nsee console printout for diagnostics\n"),
		   curr_func.src);
	if (curr_func_no >= 0)
	{						// detach old function
	  if (funclist[curr_func_no]->handle)
	  {
#ifdef DEBUG
	    printf ("BlendProgram::process_buffer detaching function %d dlclose(%s)\n",
		    curr_func_no, funclist[curr_func_no]->src);
#endif
	    dlclose (funclist[curr_func_no]->handle);
	  }
#ifdef DEBUG
	  printf ("BlendProgram::process_buffer removing function %d (%s)\n",
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
	printf ("BlendProgram::process_buffer\n   curr_func.tstamp %s",
		ctime(&curr_func.tstamp));
	printf ("   tstamp %s   relinking %s\n", ctime(&tstamp), str);
#endif
	if (curr_func_no >= 0 && funclist[curr_func_no]->handle)
	{						// detach old function
#ifdef DEBUG
	  printf ("BlendProgram::process_buffer detaching function %d dlclose(%s)\n",
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
	printf ("BlendProgram::process_buffer dlopen(%s)=%p\n",
		str, curr_func.handle);
#endif
	if (curr_func.handle)	// inquire necessary extern entry points
	{			// bpProc is mandatory, bpInit optional
	  curr_func.init = (BPF_init) dlsym (curr_func.handle, "bpInit");
	  if (curr_func.init == NULL)	// not a problem, we can continue
	    printf (_("Blend Program: optional entry point \"bpInit\" for program %s not found:\n%s\n"),
		    str, dlerror());
	  curr_func.proc = (BPF_proc) dlsym (curr_func.handle, "bpProc");
#ifdef DEBUG
	  printf ("BlendProgram::process_buffer dlsym(%s) init=%p proc=%p\n",
		  curr_func.src, curr_func.init, curr_func.proc);
#endif
	  if (curr_func.proc == NULL)	// nothing to do if this not working
	  {
	    eprintf (_("Blend Program: entry point \"bpProc\" for program %s not found:\n%s\n"),
		     str, dlerror());
#ifdef DEBUG
	    printf ("BlendProgram::process_buffer dlclose(%s)\n",
		    curr_func.src);
#endif
	    dlclose (curr_func.handle);
	    curr_func.handle = 0;
	    curr_func.init   = 0;
	  }
	}
	else
	  eprintf (_("Blend Program: dynamic load of program %s failed:\n%s\n"),
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
	    printf ("BlendProgram::process_buffer function %d (%s) updated\n   timestamp %s",
		    curr_func_no, curr_func.src, ctime(&curr_func.tstamp));
#endif
	  }
	  else					// remove outdated function
	  {
#ifdef DEBUG
	    printf ("BlendProgram::process_buffer removing function %d (%s)\n",
		    curr_func_no, curr_func.src);
#endif
	    funclist.remove_object_number (curr_func_no);
	    curr_func_no = -1;
	  }
	}
	else if (curr_func.proc)	// add new linked function to cache
	{
	  curr_func_no = funclist.total;
	  ptr = new BlendProgramFunc;
	  funclist.append (ptr);
	  strcpy (ptr->src, curr_func.src);
	  ptr->handle = curr_func.handle;
	  ptr->proc   = curr_func.proc;
	  ptr->init   = curr_func.init;
	  ptr->tstamp = curr_func.tstamp;
#ifdef DEBUG
	  printf ("BlendProgram::process_buffer function %d (%s) appended\n   timestamp %s",
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
	printf ("BlendProgram::process_buffer function %s does not need relinking\n   cache number %d timestamp %s",
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
      eprintf (_("Blend Program: cannot execute program %s:\nrequires %d tracks to process, has only %d tracks\n"),
	       curr_func.src, min_layers, layers);
    return 0;				// too few tracks to do anything
  }
  if (color_work == BlendProgramConfig::AUTO) color_work = color_arg;
  if (color_work == BlendProgramConfig::AUTO)
    color_work = BlendProgramConfig::PROJECT; // still not defined, dont change
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
      engine = new BlendProgramEngine (this,
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
// all the conversions will be rolled back in the reverse order.
// This universal function is called via loadbalance multithreading engine
// as well as directly if parallelism not requested or not supported

void BlendProgram::process_frames (int y1, int y2)
{
  float r[layers], g[layers], b[layers], a[layers];
  float rk, gk, bk, yk, uk, vk;
  int k, l, start, step;

  if (config.direction == BlendProgramConfig::BOTTOM_FIRST)
  {
    start = layers-1;
    step  = -1;
  }
  else
  {
    start = 0;
    step  = 1;
  }

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
	  if (color_work == BlendProgramConfig::YUV)
	  {
	    YUV::yuv.rgb_to_yuv_f (row[l][0], row[l][1], row[l][2],
				   r[k],	// Y pixel to blend
				   g[k],	// U pixel
				   b[k]);	// V pixel
	    yk = yuv_y;				// user's key color (YUV)
	    uk = yuv_u;
	    vk = yuv_v;
	  }
	  else if (color_work == BlendProgramConfig::HSV)
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
	  k += step;
	}	// scan tracks for l = 0 .. layers
	curr_func.proc (layers, r, g, b, a, yk, uk, vk, key_a,
			j, i, width, height, has_alpha); // call user function
	k = start;
	for (l=0; l<layers; l++)	// convert modified args back to frames
	{
	  if (clip_colors) CLAMP (a[k], 0, 1);
	  if (color_work == BlendProgramConfig::YUV)
	  {
	    if (clip_colors)
	    {
	      CLAMP (r[k],  0,   1);
	      CLAMP (g[k], -0.5, 0.5);
	      CLAMP (b[k], -0.5, 0.5);
	    }
	    if (! (isfinite(r[k]) && isfinite(g[k]) &&
		   isfinite(b[k]) && isfinite(a[k])))
	    {	// substitute NaN or unclipped infinity with user's color (YUV)
	      r[k] = yuv_y;
	      g[k] = yuv_u;
	      b[k] = yuv_v;
	      a[k] = key_a;
	    }
	    YUV::yuv.yuv_to_rgb_f (row[l][0], row[l][1], row[l][2],
				   r[k],	// Y
				   g[k],	// U
				   b[k]);	// V
	  }
	  else if (color_work == BlendProgramConfig::HSV)
	  {
	    if (clip_colors)
	    {
	      if (isfinite(r[k]) && (r[k] < 0 || r[k] >= 360))
		r[k] -= floor(r[k]/360)*360;	// cannot clamp infinity here
	      CLAMP (g[k], 0, 1);
	      CLAMP (b[k], 0, 1);
	    }
	    if (! (isfinite(r[k]) && isfinite(g[k]) &&
		   isfinite(b[k]) && isfinite(a[k])))
	    {	// substitute NaN or unclipped infinity with user's color (HSV)
	      r[k] = hsv_h;
	      g[k] = hsv_s;
	      b[k] = hsv_v;
	      a[k] = key_a;
	    }
	    HSV::hsv_to_rgb (row[l][0], row[l][1], row[l][2],
			     r[k],		// H
			     g[k],		// S
			     b[k]);		// V
	  }
	  else	// either RGB or by PROJECT, no change, clip only
	  {
	    if (clip_colors)
	    {
	      CLAMP (r[k], 0, 1);
	      CLAMP (g[k], 0, 1);
	      CLAMP (b[k], 0, 1);
	    }
	    if (! (isfinite(r[k]) && isfinite(g[k]) &&
		   isfinite(b[k]) && isfinite(a[k])))
	    {	// substitute NaN or unclipped infinity with user's color (RGB)
	      r[k] = rgb_r;
	      g[k] = rgb_g;
	      b[k] = rgb_b;
	      a[k] = key_a;
	    }
	    row[l][0] = r[k];
	    row[l][1] = g[k];
	    row[l][2] = b[k];
	  }	// if color_work
	  if (! has_alpha)			// evtl simulate alpha channel
	  {
	    row[l][0] *= a[k];
	    row[l][1] *= a[k];
	    row[l][2] *= a[k];
	  }
	  if (has_alpha)			// store real alpha channel
	  {
	    row[l][3] = a[k];
	    row[l] += 4;
	  }
	  else row[l] += 3;			// no alpha channel
	  k += step;
	}	// scan tracks for l = 0 .. layers
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
	  if (color_work == BlendProgramConfig::YUV)
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
	  else if (color_work == BlendProgramConfig::HSV)
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
	  k += step;
	}	// scan tracks for l = 0 .. layers
	curr_func.proc (layers, r, g, b, a, yk, uk, vk, key_a,
			j, i, width, height, has_alpha); // call user function
	k = start;
	for (l=0; l<layers; l++)	// convert modified args back to frames
	{
	  if (clip_colors) CLAMP (a[k], 0, 1);
	  if (color_work == BlendProgramConfig::YUV)
	  {
	    if (clip_colors)
	    {
	      CLAMP (r[k],  0,   1);
	      CLAMP (g[k], -0.5, 0.5);
	      CLAMP (b[k], -0.5, 0.5);
	    }
	    if (! (isfinite(r[k]) && isfinite(g[k]) &&
		   isfinite(b[k]) && isfinite(a[k])))
	    {	// substitute NaN or unclipped infinity with user's color (YUV)
	      r[k] = yuv_y;
	      g[k] = yuv_u;
	      b[k] = yuv_v;
	      a[k] = key_a;
	    }
	    YUV::yuv.yuv_to_rgb_f (rk, gk, bk,
				   r[k],	// Y
				   g[k],	// U
				   b[k]);	// V
	  }
	  else if (color_work == BlendProgramConfig::HSV)
	  {
	    if (clip_colors)
	    {
	      if (isfinite(r[k]) && (r[k] < 0 || r[k] >= 360))
		r[k] -= floor(r[k]/360)*360;	// cannot clamp infinity here
	      CLAMP (g[k], 0, 1);
	      CLAMP (b[k], 0, 1);
	    }
	    if (! (isfinite(r[k]) && isfinite(g[k]) &&
		   isfinite(b[k]) && isfinite(a[k])))
	    {	// substitute NaN or unclipped infinity with user's color (HSV)
	      r[k] = hsv_h;
	      g[k] = hsv_s;
	      b[k] = hsv_v;
	      a[k] = key_a;
	    }
	    HSV::hsv_to_rgb (rk, gk, bk,
			     r[k],		// H
			     g[k],		// S
			     b[k]);		// V
	  }
	  else	// either RGB or by PROJECT, no change, clip only
	  {
	    if (clip_colors)
	    {
	      CLAMP (r[k], 0, 1);
	      CLAMP (g[k], 0, 1);
	      CLAMP (b[k], 0, 1);
	    }
	    if (! (isfinite(r[k]) && isfinite(g[k]) &&
		   isfinite(b[k]) && isfinite(a[k])))
	    {	// substitute NaN or unclipped infinity with user's color (RGB)
	      r[k] = rgb_r;
	      g[k] = rgb_g;
	      b[k] = rgb_b;
	      a[k] = key_a;
	    }
	    rk = r[k];
	    gk = g[k];
	    bk = b[k];
	  }	// if color_work
	  if (! has_alpha)			// evtl simulate alpha channel
	  {
	    rk *= a[k];
	    gk *= a[k];
	    bk *= a[k];
	  }
	  row[l][0] = (unsigned char) CLIP (rk*255, 0, 255); // reformat / clip
	  row[l][1] = (unsigned char) CLIP (gk*255, 0, 255);
	  row[l][2] = (unsigned char) CLIP (bk*255, 0, 255);
	  if (has_alpha)			// store real alpha channel
	  {
	    row[l][3] = (unsigned char) CLIP (a[k]*255, 0, 255);
	    row[l] += 4;
	  }
	  else row[l] += 3;			// no alpha channel
	  k += step;
	}	// scan tracks for l = 0 .. layers
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
	  if (color_work == BlendProgramConfig::RGB)
	  {
	    YUV::yuv.yuv_to_rgb_f (r[k], g[k], b[k], // RGB pixel to blend
				   (float)row[l][0]/255,
				   ((float)row[l][1]-128)/256,
				   ((float)row[l][2]-128)/256);
	    yk = rgb_r;				// user's key color (RGB)
	    uk = rgb_g;
	    vk = rgb_b;
	  }
	  else if (color_work == BlendProgramConfig::HSV)
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
	  k += step;
	}	// scan tracks for l = 0 .. layers
	curr_func.proc (layers, r, g, b, a, yk, uk, vk, key_a,
			j, i, width, height, has_alpha); // call user function
	k = start;
	for (l=0; l<layers; l++)	// convert modified args back to frames
	{
	  if (clip_colors) CLAMP (a[k], 0, 1);
	  if (color_work == BlendProgramConfig::RGB)
	  {
	    if (clip_colors)
	    {
	      CLAMP (r[k], 0, 1);
	      CLAMP (g[k], 0, 1);
	      CLAMP (b[k], 0, 1);
	    }
	    if (! (isfinite(r[k]) && isfinite(g[k]) &&
		   isfinite(b[k]) && isfinite(a[k])))
	    {	// substitute NaN or unclipped infinity with user's color (RGB)
	      r[k] = rgb_r;
	      g[k] = rgb_g;
	      b[k] = rgb_b;
	      a[k] = key_a;
	    }
	    YUV::yuv.rgb_to_yuv_f (r[k], g[k], b[k], yk, uk, vk);
	  }
	  else if (color_work == BlendProgramConfig::HSV)
	  {
	    if (clip_colors)
	    {
	      if (isfinite(r[k]) && (r[k] < 0 || r[k] >= 360))
		r[k] -= floor(r[k]/360)*360;	// cannot clamp infinity here
	      CLAMP (g[k], 0, 1);
	      CLAMP (b[k], 0, 1);
	    }
	    if (! (isfinite(r[k]) && isfinite(g[k]) &&
		   isfinite(b[k]) && isfinite(a[k])))
	    {	// substitute NaN or unclipped infinity with user's color (HSV)
	      r[k] = hsv_h;
	      g[k] = hsv_s;
	      b[k] = hsv_v;
	      a[k] = key_a;
	    }			  //     H     S     V
	    HSV::hsv_to_rgb (rk, gk, bk, r[k], g[k], b[k]);
	    if (clip_colors)
	    {
	      CLAMP (r[k], 0, 1);
	      CLAMP (g[k], 0, 1);
	      CLAMP (b[k], 0, 1);
	    }
	    YUV::yuv.rgb_to_yuv_f (rk, gk, bk, yk, uk, vk);
	  }
	  else	// either YUV or by PROJECT, no change, clip only
	  {
	    if (clip_colors)
	    {
	      CLAMP (r[k],  0,   1);
	      CLAMP (g[k], -0.5, 0.5);
	      CLAMP (b[k], -0.5, 0.5);
	    }
	    if (! (isfinite(r[k]) && isfinite(g[k]) &&
		   isfinite(b[k]) && isfinite(a[k])))
	    {	// substitute NaN or unclipped infinity with user's color (YUV)
	      r[k] = yuv_y;
	      g[k] = yuv_u;
	      b[k] = yuv_v;
	      a[k] = key_a;
	    }
	    yk = r[k];
	    uk = g[k];
	    vk = b[k];
	  }	// if color_work
	  if (! has_alpha)			// evtl simulate alpha channel
	  {
	    yk *= a[k];
	    uk *= a[k];
	    vk *= a[k];
	  }
	  row[l][0] = (unsigned char) CLIP (yk*255,       0, 255); // reformat
	  row[l][1] = (unsigned char) CLIP ((uk+0.5)*256, 0, 255);
	  row[l][2] = (unsigned char) CLIP ((vk+0.5)*256, 0, 255);
	  if (has_alpha)			// store real alpha channel
	  {
	    row[l][3] = (unsigned char) CLIP (a[k]*255, 0, 255);
	    row[l] += 4;
	  }
	  else row[l] += 3;			// no alpha channel
	  k += step;
	}	// scan tracks for l = 0 .. layers
      }		// scan pixels for j = 0 .. width
    }		// scan rows for i = y1 .. y2
    break;
  default:
    break;
  }		// switch color_proj
}						// WHEW !!!

void BlendProgram::save_data(KeyFrame *keyframe)
{
  FileXML output;

  output.set_shared_output(keyframe->xbuf);

  output.tag.set_title("BLEND_PROGRAM");
  output.tag.set_property("FUNCNAME",   config.funcname);
  output.tag.set_property("PARALLEL",   config.parallel);
  output.tag.set_property("DIRECTION",  config.direction);
  output.tag.set_property("COLORSPACE", config.colorspace);
  output.tag.set_property("CLIPCOLORS", config.clipcolors);
  output.tag.set_property("RED",        config.red);
  output.tag.set_property("GREEN",      config.green);
  output.tag.set_property("BLUE",       config.blue);
  output.tag.set_property("ALPHA",      config.alpha);
  output.append_tag();
  output.tag.set_title("/BLEND_PROGRAM");
  output.append_tag();
  output.append_newline();
  output.terminate_string();
}

void BlendProgram::read_data(KeyFrame *keyframe)
{
  FileXML input;

  input.set_shared_input(keyframe->xbuf);

  while(!input.read_tag())
  {
    if(input.tag.title_is("BLEND_PROGRAM"))
    {
      input.tag.get_property("FUNCNAME", config.funcname);
      config.parallel  = input.tag.get_property("PARALLEL",  config.parallel);
      config.direction = input.tag.get_property("DIRECTION", config.direction);
      config.colorspace =
	input.tag.get_property("COLORSPACE", config.colorspace);
      config.clipcolors =
	input.tag.get_property("CLIPCOLORS", config.clipcolors);
      config.red   = input.tag.get_property("RED",   config.red);
      config.green = input.tag.get_property("GREEN", config.green);
      config.blue  = input.tag.get_property("BLUE",  config.blue);
      config.alpha = input.tag.get_property("ALPHA", config.alpha);
    }
  }
}

void BlendProgram::update_gui()
{
  if( ! thread ) return;
  if( ! (load_configuration() || inspect_configuration) ) return;
  inspect_configuration = 0;	// update once after change or after creation
  thread->window->lock_window("BlendProgram::update_gui");
  BlendProgramWindow *window = (BlendProgramWindow*)thread->window;

  window->funcname->update(config.funcname);
  window->parallel->update(config.parallel);
  window->clipcolors->update(config.clipcolors);
  window->direction->set_text(
    BlendProgramConfig::direction_to_text(config.direction));
  window->colorspace->set_text(
    BlendProgramConfig::colorspace_to_text(config.colorspace));
  window->update_key_sample();
  window->alpha_text->update(config.alpha);
  window->key_alpha->update(config.alpha);

  thread->window->unlock_window();
}

////////////////////////////////////////////
// Multithreaded processing stuff
////////////////////////////////////////////

BlendProgramEngine::BlendProgramEngine(BlendProgram *plugin, 
				       int total_clients, 
				       int total_packages)
  : LoadServer(total_clients, total_packages)
{
  this->plugin = plugin;
}

void BlendProgramEngine::init_packages ()
{
  for (int i=0; i<get_total_packages(); i++)
  {
    BlendProgramPackage *pkg = (BlendProgramPackage *) get_package (i);
    pkg->y1 = plugin->height *  i      / get_total_packages ();
    pkg->y2 = plugin->height * (i + 1) / get_total_packages ();
  }
}

LoadClient *BlendProgramEngine::new_client ()
{
  return new BlendProgramUnit (plugin, this);
}

LoadPackage *BlendProgramEngine::new_package ()
{
  return new BlendProgramPackage;
}

BlendProgramPackage::BlendProgramPackage()
  : LoadPackage()
{
}

BlendProgramUnit::BlendProgramUnit (BlendProgram *plugin,
				    BlendProgramEngine *engine)
  : LoadClient (engine)
{
  this->plugin = plugin;
  this->engine = engine;
}

void BlendProgramUnit::process_package(LoadPackage *package)
{
  BlendProgramPackage *pkg = (BlendProgramPackage *) package;

  plugin->process_frames (pkg->y1, pkg->y2);
}
