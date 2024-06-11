/*
 * CINELERRA
 * Copyright (C) 2012-2024 Adam Williams <broadcast at earthling dot net>
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

// original chromakey HSV author:
// https://www.mail-archive.com/cinelerra@skolelinux.no/msg00379.html
// http://jcornet.free.fr/linux/chromakey.html

// He was trying to replicate the Avid SpectraMatte:
// https://resources.avid.com/SupportFiles/attach/Symphony_Effects_and_CC_Guide_v5.5.pdf
// page 691
// but the problem seems to be harder than he thought

// A rewrite got a slightly different spill light processing
// but still not very useful:
// https://github.com/vanakala/cinelerra-cve/blob/master/plugins/chromakeyhsv/chromakey.C

// The current implementation is most of the Spectramatte algorithm.
// The only way to test it is to use the color swatch plugin.
// Fix brightness to test the saturation wedge.
// Fix saturation to test the brightness wedge.

// There are boundary artifacts caused by the color swatch showing a slice
// of a cube.
// If out slope > 0, constant 0% saturation or 0% brightness is all wedge.
// If out slope == 0, constant 0% saturation or constant 0% brightness has no wedge
// The general idea is if it's acting weird, switch between constant brightness
// & constant saturation.

// TODO: integrated color swatch & alpha blur, but it takes a lot of space in the GUI.

#include "bcdisplayinfo.h"
#include "bcsignals.h"
#include "chromakey.h"
#include "clip.h"
#include "bchash.h"
#include "filexml.h"
#include "guicast.h"
#include "keyframe.h"
#include "language.h"
#include "loadbalance.h"
#include "playback3d.h"
#include "bccolors.h"
#include "pluginvclient.h"
#include "vframe.h"

#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define WINDOW_W xS(550)
#define WINDOW_H yS(600)
#define SLIDER_W xS(250)
#define MAX_SLOPE 100.0
#define MAX_VALUE 100.0

ChromaKeyConfig::ChromaKeyConfig ()
{
  red   = 0.0;
  green = 1.0;
  blue  = 0.0;

  min_brightness   =  50.0;
  max_brightness   = 100.0;
  tolerance        =  15.0;
  saturation_start =   0.0;
  saturation_line  =  50.0;

  in_slope     = 2;
  out_slope    = 2;
  alpha_offset = 0;

  spill_saturation = 0.0;
  spill_angle      = 0.0;
  desaturate_only  = 0;

  show_mask = 0;

  undo_red               = red;
  undo_green             = green;
  undo_blue              = blue;
  undo_min_brightness    = min_brightness;
  undo_max_brightness    = max_brightness;
  undo_tolerance         = tolerance;
  undo_saturation_start  = saturation_start;
  undo_saturation_line   = saturation_line;
  undo_in_slope          = in_slope;
  undo_out_slope         = out_slope;
  undo_alpha_offset      = alpha_offset;
  undo_spill_saturation  = spill_saturation;
  undo_spill_angle       = spill_angle;
  undo_desaturate_only   = desaturate_only;
  undo_show_mask         = show_mask;

  store_red              = red;
  store_green            = green;
  store_blue             = blue;
  store_min_brightness   = min_brightness;
  store_max_brightness   = max_brightness;
  store_tolerance        = tolerance;
  store_saturation_start = saturation_start;
  store_saturation_line  = saturation_line;
  store_in_slope         = in_slope;
  store_out_slope        = out_slope;
  store_alpha_offset     = alpha_offset;
  store_spill_saturation = spill_saturation;
  store_spill_angle      = spill_angle;
  store_desaturate_only  = desaturate_only;
  store_show_mask        = show_mask;
}

void ChromaKeyConfig::reset(int clear)
{
  switch(clear)
  {
  case RESET_RGB:
    red   = 0.0;
    green = 1.0;
    blue  = 0.0;
    break;
  case RESET_MIN_BRIGHTNESS:
    min_brightness = 50.0;
    break;
  case RESET_MAX_BRIGHTNESS:
    max_brightness = 100.0;
    break;
  case RESET_TOLERANCE:
    tolerance = 15.0;
    break;
  case RESET_SATURATION_START:
    saturation_start = 0.0;
    break;
  case RESET_SATURATION_LINE:
    saturation_line = 50.0;
    break;
  case RESET_IN_SLOPE:
    in_slope = 2;
    break;
  case RESET_OUT_SLOPE:
    out_slope = 2;
    break;
  case RESET_ALPHA_OFFSET:
    alpha_offset = 0;
    break;
  case RESET_SPILL_SATURATION:
    spill_saturation = 0.0;
    break;
  case RESET_SPILL_ANGLE:
    spill_angle = 0.0;
    break;
  case RESET_ALL:
  case RESET_DEFAULT_SETTINGS:
  default:
    undo_red              = red;
    undo_green            = green;
    undo_blue             = blue;
    undo_min_brightness   = min_brightness;
    undo_max_brightness   = max_brightness;
    undo_tolerance        = tolerance;
    undo_saturation_start = saturation_start;
    undo_saturation_line  = saturation_line;
    undo_in_slope         = in_slope;
    undo_out_slope        = out_slope;
    undo_alpha_offset     = alpha_offset;
    undo_spill_saturation = spill_saturation;
    undo_spill_angle      = spill_angle;
    undo_desaturate_only  = desaturate_only;
    undo_show_mask        = show_mask;

    red   = 0.0;
    green = 1.0;
    blue  = 0.0;

    min_brightness   =  50.0;
    max_brightness   = 100.0;
    tolerance        =  15.0;
    saturation_start =   0.0;
    saturation_line  =  50.0;

    in_slope     = 2;
    out_slope    = 2;
    alpha_offset = 0;

    spill_saturation = 0.0;
    spill_angle      = 0.0;
    desaturate_only  = 0;

    show_mask = 0;
    break;
  }
}


void
ChromaKeyConfig::copy_from (ChromaKeyConfig & src)
{
  red              = src.red;
  green            = src.green;
  blue             = src.blue;
  spill_saturation = src.spill_saturation;
  spill_angle      = src.spill_angle;
  min_brightness   = src.min_brightness;
  max_brightness   = src.max_brightness;
  saturation_start = src.saturation_start;
  saturation_line  = src.saturation_line;
  tolerance        = src.tolerance;
  in_slope         = src.in_slope;
  out_slope        = src.out_slope;
  alpha_offset     = src.alpha_offset;
  show_mask        = src.show_mask;
  desaturate_only  = src.desaturate_only;

  undo_red              = src.red;
  undo_green            = src.green;
  undo_blue             = src.blue;
  undo_min_brightness   = src.min_brightness;
  undo_max_brightness   = src.max_brightness;
  undo_tolerance        = src.tolerance;
  undo_saturation_start = src.saturation_start;
  undo_saturation_line  = src.saturation_line;
  undo_in_slope         = src.in_slope;
  undo_out_slope        = src.out_slope;
  undo_alpha_offset     = src.alpha_offset;
  undo_spill_saturation = src.spill_saturation;
  undo_spill_angle      = src.spill_angle;
  undo_desaturate_only  = src.desaturate_only;
  undo_show_mask        = src.show_mask;
}

int
ChromaKeyConfig::equivalent (ChromaKeyConfig & src)
{
  return (EQUIV (red,              src.red)              &&
	  EQUIV (green,            src.green)            &&
	  EQUIV (blue,             src.blue)             &&
	  EQUIV (spill_saturation, src.spill_saturation) &&
	  EQUIV (spill_angle,      src.spill_angle)      &&
	  EQUIV (min_brightness,   src.min_brightness)   &&
	  EQUIV (max_brightness,   src.max_brightness)   &&
	  EQUIV (saturation_start, src.saturation_start) &&
	  EQUIV (saturation_line,  src.saturation_line)  &&
	  EQUIV (tolerance,        src.tolerance)        &&
	  EQUIV (in_slope,         src.in_slope)         &&
	  EQUIV (out_slope,        src.out_slope)        &&
	  EQUIV (alpha_offset,     src.alpha_offset)     &&
	  show_mask       == src.show_mask &&
	  desaturate_only == src.desaturate_only);
}

void
ChromaKeyConfig::interpolate (ChromaKeyConfig & prev,
			      ChromaKeyConfig & next,
			      int64_t prev_frame,
			      int64_t next_frame,
			      int64_t current_frame)
{
  double next_scale =
    (double) (current_frame - prev_frame) / (next_frame - prev_frame);
  double prev_scale =
    (double) (next_frame - current_frame) / (next_frame - prev_frame);

  this->red   = prev.red   * prev_scale + next.red   * next_scale;
  this->green = prev.green * prev_scale + next.green * next_scale;
  this->blue  = prev.blue  * prev_scale + next.blue  * next_scale;
  this->spill_saturation =
    prev.spill_saturation * prev_scale + next.spill_saturation * next_scale;
  this->spill_angle =
    prev.spill_angle * prev_scale + next.tolerance * next_scale;
  this->min_brightness =
    prev.min_brightness * prev_scale + next.min_brightness * next_scale;
  this->max_brightness =
    prev.max_brightness * prev_scale + next.max_brightness * next_scale;
  this->saturation_start =
    prev.saturation_start * prev_scale + next.saturation_start * next_scale;
  this->saturation_line =
    prev.saturation_line * prev_scale + next.saturation_line * next_scale;
  this->tolerance = prev.tolerance * prev_scale + next.tolerance * next_scale;
  this->in_slope  = prev.in_slope  * prev_scale + next.in_slope  * next_scale;
  this->out_slope = prev.out_slope * prev_scale + next.out_slope * next_scale;
  this->alpha_offset =
    prev.alpha_offset * prev_scale + next.alpha_offset * next_scale;
  this->show_mask       = prev.show_mask;
  this->desaturate_only = prev.desaturate_only;

  this->undo_red              = this->red;
  this->undo_green            = this->green;
  this->undo_blue             = this->blue;
  this->undo_min_brightness   = this->min_brightness;
  this->undo_max_brightness   = this->max_brightness;
  this->undo_tolerance        = this->tolerance;
  this->undo_saturation_start = this->saturation_start;
  this->undo_saturation_line  = this->saturation_line;
  this->undo_in_slope         = this->in_slope;
  this->undo_out_slope        = this->out_slope;
  this->undo_alpha_offset     = this->alpha_offset;
  this->undo_spill_saturation = this->spill_saturation;
  this->undo_spill_angle      = this->spill_angle;
  this->undo_desaturate_only  = this->desaturate_only;
  this->undo_show_mask        = this->show_mask;
}

int
ChromaKeyConfig::get_color ()
{
  int red   = (int) (CLIP (this->red,   0, 1) * 0xff);
  int green = (int) (CLIP (this->green, 0, 1) * 0xff);
  int blue  = (int) (CLIP (this->blue,  0, 1) * 0xff);
  return (red << 16) | (green << 8) | blue;
}



ChromaKeyWindow::ChromaKeyWindow (ChromaKeyAvid * plugin)
  : PluginClientWindow(plugin,
		       WINDOW_W, 
		       WINDOW_H, 
		       WINDOW_W, 
		       WINDOW_H, 
		       0)
{
  this->plugin = plugin;
  color_thread = 0;
}

ChromaKeyWindow::~ChromaKeyWindow ()
{
  delete color_thread;
}

void ChromaKeyWindow::create_objects ()
{
  int xs10 = xS(10), xs20 = xS(20), xs100 = xS(100), wslid = SLIDER_W;
  int ys10 = yS(10), ys20 = yS(20), ys30 = yS(30), ys40 = yS(40), ys50 = yS(50);
  int y = ys10, x2 = xS(160), x3 = xS(260), xs5 = xS(5);
  int x = xs10;
  int clr_x = get_w()-x - xS(22); // note: clrBtn_w = 22

  BC_Title *title;
  BC_Bar *bar;
  BC_TitleBar *title_bar;

// Color section
  add_subwindow (title_bar =
		 new BC_TitleBar(x, y, get_w()-2*x, xs20, xs10, _("Color")));
  y += ys20;

  add_subwindow (color = new ChromaKeyColor (plugin, this, x, y));
  // Info for Sample rectangle:       x_slider w_slider w_sample
  //                                        \       |      /    y,   w,     h
  add_subwindow (sample = new BC_SubWindow (x3 + wslid - xs100, y, xs100, ys50));
  y += ys30;
  add_subwindow (use_colorpicker = new ChromaKeyUseColorPicker (plugin, this, x, y));
  y += ys30;
  add_subwindow (show_mask =
		 new ChromaKeyToggle (plugin, x, y, &plugin->config.show_mask,
				      _("Show Mask")));

// Key parameters section
  y += ys30;
  add_subwindow (title_bar =
		 new BC_TitleBar (x, y, get_w()-2*x, xs20, xs10,
				  _("Key parameters")));
  y += ys20;
  add_subwindow (title = new BC_Title (x, y, _("Hue Tolerance:")));
  tolerance_text = new ChromaKeyText (plugin, this, 0, x+x2, y, 0, MAX_VALUE,
				      &plugin->config.tolerance);
  tolerance_text->create_objects();
  tolerance = new ChromaKeySlider (plugin, tolerance_text, x3, y, wslid,
				   0, MAX_VALUE, &plugin->config.tolerance);
  add_subwindow (tolerance);
  tolerance_text->slider = tolerance;
  clr_x = x3 + tolerance->get_w() + x;
  add_subwindow (tolerance_clr =
		 new ChromaKeyClr (plugin, this, clr_x, y, RESET_TOLERANCE));
  y += ys30;

  add_subwindow (title = new BC_Title (x, y, _("Min. Brightness:")));
  min_brightness_text = new ChromaKeyText (plugin, this, 0, x+x2, y,
					   0, MAX_VALUE,
					   &plugin->config.min_brightness);
  min_brightness_text->create_objects();
  min_brightness = new ChromaKeySlider (plugin, min_brightness_text,
					x3, y, wslid, 0, MAX_VALUE,
					&plugin->config.min_brightness);
  add_subwindow(min_brightness);
  min_brightness_text->slider = min_brightness;
  add_subwindow (min_brightness_clr =
		 new ChromaKeyClr (plugin, this, clr_x, y,
				   RESET_MIN_BRIGHTNESS));
  y += ys30;

  add_subwindow (title = new BC_Title (x, y, _("Max. Brightness:")));
  max_brightness_text = new ChromaKeyText (plugin, this, 0, x+x2, y,
					   0, MAX_VALUE,
					   &plugin->config.max_brightness);
  max_brightness_text->create_objects();
  max_brightness = new ChromaKeySlider (plugin, max_brightness_text,
					x3, y, wslid, 0, MAX_VALUE,
					&plugin->config.max_brightness);
  add_subwindow(max_brightness);
  max_brightness_text->slider = max_brightness;
  add_subwindow (max_brightness_clr =
		 new ChromaKeyClr (plugin, this, clr_x, y,
				   RESET_MAX_BRIGHTNESS));
  y += ys30;

  add_subwindow (title = new BC_Title (x, y, _("Saturation Start:")));
  saturation_start_text = new ChromaKeyText (plugin, this, 0, x+x2, y,
					     0, MAX_VALUE,
					     &plugin->config.saturation_start);
  saturation_start_text->create_objects();
  saturation_start = new ChromaKeySlider (plugin, saturation_start_text,
					  x3, y, wslid, 0, MAX_VALUE,
					  &plugin->config.saturation_start);
  add_subwindow(saturation_start);
  saturation_start_text->slider = saturation_start;
  add_subwindow (saturation_start_clr =
		 new ChromaKeyClr (plugin, this, clr_x, y,
				   RESET_SATURATION_START));
  y += ys30;

  add_subwindow (title = new BC_Title (x, y, _("Saturation Line:")));
  saturation_line_text = new ChromaKeyText (plugin, this, 0, x+x2, y,
					    0, MAX_VALUE,
					    &plugin->config.saturation_line);
  saturation_line_text->create_objects();
  saturation_line = new ChromaKeySlider (plugin, saturation_line_text,
					 x3, y, wslid, 0, MAX_VALUE,
					 &plugin->config.saturation_line);
  add_subwindow(saturation_line);
  saturation_line_text->slider = saturation_line;
  add_subwindow (saturation_line_clr =
		 new ChromaKeyClr (plugin, this, clr_x, y,
				   RESET_SATURATION_LINE));
  y += ys40;

// Mask tweaking section
  add_subwindow (title_bar =
		 new BC_TitleBar (x, y, get_w()-2*x, xs20, xs10,
				  _("Mask tweaking")));
  y += ys20;
  add_subwindow (title = new BC_Title (x, y, _("In Slope:")));
  in_slope_text = new ChromaKeyText (plugin, this, 0, x+x2, y, 0, MAX_SLOPE,
				     &plugin->config.in_slope);
  in_slope_text->create_objects();
  in_slope = new ChromaKeySlider (plugin, in_slope_text, x3, y, wslid,
				  0, MAX_SLOPE, &plugin->config.in_slope);
  add_subwindow(in_slope);
  in_slope_text->slider = in_slope;
  add_subwindow (in_slope_clr =
		 new ChromaKeyClr (plugin, this, clr_x, y, RESET_IN_SLOPE));
  y += ys30;

  add_subwindow (title = new BC_Title (x, y, _("Out Slope:")));
  out_slope_text = new ChromaKeyText (plugin, this, 0, x+x2, y, 0, MAX_SLOPE,
				      &plugin->config.out_slope);
  out_slope_text->create_objects();
  out_slope = new ChromaKeySlider (plugin, out_slope_text, x3, y, wslid,
				   0, MAX_SLOPE, &plugin->config.out_slope);
  add_subwindow(out_slope);
  out_slope_text->slider = out_slope;
  add_subwindow (out_slope_clr =
		 new ChromaKeyClr (plugin, this, clr_x, y, RESET_OUT_SLOPE));
  y += ys30;

  add_subwindow (title = new BC_Title (x, y, _("Alpha Offset:")));
  alpha_offset_text = new ChromaKeyText (plugin, this, 0, x+x2, y,
					 -MAX_VALUE, MAX_VALUE,
					 &plugin->config.alpha_offset);
  alpha_offset_text->create_objects();
  alpha_offset = new ChromaKeySlider (plugin, alpha_offset_text, x3, y, wslid,
				      -MAX_VALUE, MAX_VALUE,
				      &plugin->config.alpha_offset);
  add_subwindow(alpha_offset);
  alpha_offset_text->slider = alpha_offset;
  add_subwindow (alpha_offset_clr =
		 new ChromaKeyClr (plugin, this, clr_x, y, RESET_ALPHA_OFFSET));
  y += ys40;

// Spill light control section
  add_subwindow (title_bar =
		 new BC_TitleBar (x, y, get_w()-2*x, xs20, xs10,
				  _("Spill light control")));
  y += ys20;
  add_subwindow (title = new BC_Title (x, y, _("Spill Saturation:")));
  spill_saturation_text = new ChromaKeyText (plugin, this, 0, x+x2, y,
					     0, MAX_VALUE,
					     &plugin->config.spill_saturation);
  spill_saturation_text->create_objects();
  spill_saturation = new ChromaKeySlider (plugin, spill_saturation_text,
					  x3, y, wslid, 0, MAX_VALUE,
					  &plugin->config.spill_saturation);
  add_subwindow(spill_saturation);
  spill_saturation_text->slider = spill_saturation;
  add_subwindow (spill_saturation_clr =
		 new ChromaKeyClr (plugin, this, clr_x, y,
				   RESET_SPILL_SATURATION));
  y += ys30;

  add_subwindow (title = new BC_Title (x, y, _("Spill Angle:")));
  spill_angle_text = new ChromaKeyText (plugin, this, 0, x+x2, y, 0, MAX_VALUE,
					&plugin->config.spill_angle);
  spill_angle_text->create_objects();
  spill_angle = new ChromaKeySlider (plugin, spill_angle_text, x3, y, wslid,
				     0, MAX_VALUE, &plugin->config.spill_angle);
  add_subwindow(spill_angle);
  spill_angle_text->slider = spill_angle;
  add_subwindow (spill_angle_clr =
		 new ChromaKeyClr (plugin, this, clr_x, y, RESET_SPILL_ANGLE));
  y += ys30;

  add_subwindow(desaturate_only =
		new ChromaKeyToggle(plugin, x, y,
				    &plugin->config.desaturate_only, 
				    _("Desaturate Only")));
  y += ys40;

// Global buttons section
  add_subwindow (bar = new BC_Bar(x, y, get_w()-2*x));
  y += ys20;
  add_subwindow (store = new ChromaKeyStore (plugin, this, x, y));
  x += store->get_w() + xs5;
  add_subwindow (recall = new ChromaKeyRecall (plugin, this, x, y));
  x += recall->get_w() + xs5;
  add_subwindow (exchange = new ChromaKeyExchange (plugin, this, x, y));
  x += exchange->get_w() + xs5;
  add_subwindow (undo = new ChromaKeyUndo (plugin, this, x, y));
  x += undo->get_w() + xs5;
  add_subwindow (reset = new ChromaKeyReset (plugin, this, x, y));

  color_thread = new ChromaKeyColorThread (plugin, this);

  update_sample ();
  show_window ();
}

void
ChromaKeyWindow::update_sample ()
{
  sample->set_color (plugin->config.get_color ());
  sample->draw_box (0, 0, sample->get_w (), sample->get_h ());
  sample->set_color (BLACK);
  sample->draw_rectangle (0, 0, sample->get_w (), sample->get_h ());
  sample->flash ();
}

void ChromaKeyWindow::done_event(int result)
{
  color_thread->close_window();
}


ChromaKeyColor::ChromaKeyColor (ChromaKeyAvid * plugin,
				ChromaKeyWindow * gui, int x, int y)
  : BC_GenericButton (x, y, _("Color..."))
{
  this->plugin = plugin;
  this->gui = gui;
}

int
ChromaKeyColor::handle_event ()
{
  gui->color_thread->start_window (plugin->config.get_color (), 0xff);
  return 1;
}



ChromaKeyText::ChromaKeyText(ChromaKeyAvid *plugin, ChromaKeyWindow *gui,
			     ChromaKeySlider *slider, int x, int y,
			     float min, float max, float *output)
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

ChromaKeyText::~ChromaKeyText()
{
}

int ChromaKeyText::handle_event()
{
  *output = atof(get_text());
  if(*output > max) *output = max;
  if(*output < min) *output = min;
  slider->update(*output);
  plugin->send_configure_change();
  return 1;
}

ChromaKeySlider::ChromaKeySlider(ChromaKeyAvid *plugin, ChromaKeyText *text,
				 int x, int y, int w,
				 float min, float max, float *output)
  : BC_FSlider(x, y, 0, w, w, min, max, *output)
{
  this->plugin = plugin;
  this->text = text;
  this->output = output;
  set_precision(0.01);
  enable_show_value(0); // Hide caption
}

ChromaKeySlider::~ChromaKeySlider()
{
}

int ChromaKeySlider::handle_event()
{
  *output = get_value();
  text->update(*output);
  plugin->send_configure_change();
  return 1;
}

ChromaKeyClr::ChromaKeyClr(ChromaKeyAvid *plugin, ChromaKeyWindow *gui,
			   int x, int y, int clear)
  : BC_Button(x, y, plugin->get_theme()->get_image_set("reset_button"))
{
  this->plugin = plugin;
  this->gui = gui;
  this->clear = clear;
}

ChromaKeyClr::~ChromaKeyClr()
{
}

int ChromaKeyClr::handle_event()
{
  plugin->config.reset(clear);
  gui->update_gui(clear);
  plugin->send_configure_change();
  return 1;
}

ChromaKeyToggle::ChromaKeyToggle (ChromaKeyAvid * plugin,
				  int x,
				  int y,
				  bool *output,
				  const char *caption)
  : BC_CheckBox (x,
		 y,
		 *output ? 1 : 0,
		 caption)
{
  this->plugin = plugin;
  this->output = output;
}

int
ChromaKeyToggle::handle_event ()
{
  *output = get_value() ? true : false;
  plugin->send_configure_change ();
  return 1;
}


ChromaKeyReset::ChromaKeyReset (ChromaKeyAvid *plugin, ChromaKeyWindow *gui,
				int x, int y)
  : BC_GenericButton(x, y, _("Reset"))
{
  this->plugin = plugin;
  this->gui = gui;
}

int ChromaKeyReset::handle_event ()
{
  plugin->config.reset(RESET_DEFAULT_SETTINGS);
  gui->update_gui(RESET_DEFAULT_SETTINGS);
  plugin->send_configure_change();
  return 1;
}

ChromaKeyStore::ChromaKeyStore (ChromaKeyAvid *plugin, ChromaKeyWindow *gui,
				int x, int y)
  : BC_GenericButton(x, y, _("Store"))
{
  this->plugin = plugin;
  this->gui = gui;
}

int ChromaKeyStore::handle_event ()
{
  plugin->config.store_red              = plugin->config.red;
  plugin->config.store_green            = plugin->config.green;
  plugin->config.store_blue             = plugin->config.blue;
  plugin->config.store_min_brightness   = plugin->config.min_brightness;
  plugin->config.store_max_brightness   = plugin->config.max_brightness;
  plugin->config.store_tolerance        = plugin->config.tolerance;
  plugin->config.store_saturation_start = plugin->config.saturation_start;
  plugin->config.store_saturation_line  = plugin->config.saturation_line;
  plugin->config.store_in_slope         = plugin->config.in_slope;
  plugin->config.store_out_slope        = plugin->config.out_slope;
  plugin->config.store_alpha_offset     = plugin->config.alpha_offset;
  plugin->config.store_spill_saturation = plugin->config.spill_saturation;
  plugin->config.store_spill_angle      = plugin->config.spill_angle;
  plugin->config.store_desaturate_only  = plugin->config.desaturate_only;
  plugin->config.store_show_mask        = plugin->config.show_mask;
  return 1;
}

ChromaKeyRecall::ChromaKeyRecall (ChromaKeyAvid *plugin, ChromaKeyWindow *gui,
				  int x, int y)
  : BC_GenericButton(x, y, _("Recall"))
{
  this->plugin = plugin;
  this->gui = gui;
}

int ChromaKeyRecall::handle_event ()
{
  plugin->config.undo_red              = plugin->config.red;
  plugin->config.undo_green            = plugin->config.green;
  plugin->config.undo_blue             = plugin->config.blue;
  plugin->config.undo_min_brightness   = plugin->config.min_brightness;
  plugin->config.undo_max_brightness   = plugin->config.max_brightness;
  plugin->config.undo_tolerance        = plugin->config.tolerance;
  plugin->config.undo_saturation_start = plugin->config.saturation_start;
  plugin->config.undo_saturation_line  = plugin->config.saturation_line;
  plugin->config.undo_in_slope         = plugin->config.in_slope;
  plugin->config.undo_out_slope        = plugin->config.out_slope;
  plugin->config.undo_alpha_offset     = plugin->config.alpha_offset;
  plugin->config.undo_spill_saturation = plugin->config.spill_saturation;
  plugin->config.undo_spill_angle      = plugin->config.spill_angle;
  plugin->config.undo_desaturate_only  = plugin->config.desaturate_only;
  plugin->config.undo_show_mask        = plugin->config.show_mask;

  plugin->config.red              = plugin->config.store_red;
  plugin->config.green            = plugin->config.store_green;
  plugin->config.blue             = plugin->config.store_blue;
  plugin->config.min_brightness   = plugin->config.store_min_brightness;
  plugin->config.max_brightness   = plugin->config.store_max_brightness;
  plugin->config.tolerance        = plugin->config.store_tolerance;
  plugin->config.saturation_start = plugin->config.store_saturation_start;
  plugin->config.saturation_line  = plugin->config.store_saturation_line;
  plugin->config.in_slope         = plugin->config.store_in_slope;
  plugin->config.out_slope        = plugin->config.store_out_slope;
  plugin->config.alpha_offset     = plugin->config.store_alpha_offset;
  plugin->config.spill_saturation = plugin->config.store_spill_saturation;
  plugin->config.spill_angle      = plugin->config.store_spill_angle;
  plugin->config.desaturate_only  = plugin->config.store_desaturate_only;
  plugin->config.show_mask        = plugin->config.store_show_mask;

  gui->update_gui(RESET_DEFAULT_SETTINGS);
  plugin->send_configure_change();
  return 1;
}

ChromaKeyExchange::ChromaKeyExchange (ChromaKeyAvid *plugin,
				      ChromaKeyWindow *gui, int x, int y)
  : BC_GenericButton(x, y, _("Exchange"))
{
  this->plugin = plugin;
  this->gui = gui;
}

#define CHROMAKEY_SWAP(what,typ,elem) \
tmp_##typ = plugin->config.what##_##elem; \
plugin->config.what##_##elem = plugin->config.elem; \
plugin->config.elem = tmp_##typ;

int ChromaKeyExchange::handle_event ()
{
  float tmp_flt;
  bool tmp_bool;

  plugin->config.undo_red              = plugin->config.red;
  plugin->config.undo_green            = plugin->config.green;
  plugin->config.undo_blue             = plugin->config.blue;
  plugin->config.undo_min_brightness   = plugin->config.min_brightness;
  plugin->config.undo_max_brightness   = plugin->config.max_brightness;
  plugin->config.undo_tolerance        = plugin->config.tolerance;
  plugin->config.undo_saturation_start = plugin->config.saturation_start;
  plugin->config.undo_saturation_line  = plugin->config.saturation_line;
  plugin->config.undo_in_slope         = plugin->config.in_slope;
  plugin->config.undo_out_slope        = plugin->config.out_slope;
  plugin->config.undo_alpha_offset     = plugin->config.alpha_offset;
  plugin->config.undo_spill_saturation = plugin->config.spill_saturation;
  plugin->config.undo_spill_angle      = plugin->config.spill_angle;
  plugin->config.undo_desaturate_only  = plugin->config.desaturate_only;
  plugin->config.undo_show_mask        = plugin->config.show_mask;

  CHROMAKEY_SWAP (store, flt,  red)
  CHROMAKEY_SWAP (store, flt,  green)
  CHROMAKEY_SWAP (store, flt,  blue)
  CHROMAKEY_SWAP (store, flt,  min_brightness)
  CHROMAKEY_SWAP (store, flt,  max_brightness)
  CHROMAKEY_SWAP (store, flt,  tolerance)
  CHROMAKEY_SWAP (store, flt,  saturation_start)
  CHROMAKEY_SWAP (store, flt,  saturation_line)
  CHROMAKEY_SWAP (store, flt,  in_slope)
  CHROMAKEY_SWAP (store, flt,  out_slope)
  CHROMAKEY_SWAP (store, flt,  alpha_offset)
  CHROMAKEY_SWAP (store, flt,  spill_saturation)
  CHROMAKEY_SWAP (store, flt,  spill_angle)
  CHROMAKEY_SWAP (store, bool, desaturate_only)
  CHROMAKEY_SWAP (store, bool, show_mask)

  gui->update_gui(RESET_DEFAULT_SETTINGS);
  plugin->send_configure_change();
  return 1;
}

ChromaKeyUndo::ChromaKeyUndo (ChromaKeyAvid *plugin, ChromaKeyWindow *gui,
			      int x, int y)
  : BC_GenericButton(x, y, _("Undo"))
{
  this->plugin = plugin;
  this->gui = gui;
}

int ChromaKeyUndo::handle_event ()
{
  //float tmp_flt;
  //bool tmp_bool;

  //CHROMAKEY_SWAP (undo, flt,  red)
  //CHROMAKEY_SWAP (undo, flt,  green)
  //CHROMAKEY_SWAP (undo, flt,  blue)
  //CHROMAKEY_SWAP (undo, flt,  min_brightness)
  //CHROMAKEY_SWAP (undo, flt,  max_brightness)
  //CHROMAKEY_SWAP (undo, flt,  tolerance)
  //CHROMAKEY_SWAP (undo, flt,  saturation_start)
  //CHROMAKEY_SWAP (undo, flt,  saturation_line)
  //CHROMAKEY_SWAP (undo, flt,  in_slope)
  //CHROMAKEY_SWAP (undo, flt,  out_slope)
  //CHROMAKEY_SWAP (undo, flt,  alpha_offset)
  //CHROMAKEY_SWAP (undo, flt,  spill_saturation)
  //CHROMAKEY_SWAP (undo, flt,  spill_angle)
  //CHROMAKEY_SWAP (undo, bool, desaturate_only)
  //CHROMAKEY_SWAP (undo, bool, show_mask)

  // better not to swap but restore undo values

  plugin->config.red              = plugin->config.undo_red;
  plugin->config.green            = plugin->config.undo_green;
  plugin->config.blue             = plugin->config.undo_blue;
  plugin->config.min_brightness   = plugin->config.undo_min_brightness;
  plugin->config.max_brightness   = plugin->config.undo_max_brightness;
  plugin->config.tolerance        = plugin->config.undo_tolerance;
  plugin->config.saturation_start = plugin->config.undo_saturation_start;
  plugin->config.saturation_line  = plugin->config.undo_saturation_line;
  plugin->config.in_slope         = plugin->config.undo_in_slope;
  plugin->config.out_slope        = plugin->config.undo_out_slope;
  plugin->config.alpha_offset     = plugin->config.undo_alpha_offset;
  plugin->config.spill_saturation = plugin->config.undo_spill_saturation;
  plugin->config.spill_angle      = plugin->config.undo_spill_angle;
  plugin->config.desaturate_only  = plugin->config.undo_desaturate_only;
  plugin->config.show_mask        = plugin->config.undo_show_mask;

  gui->update_gui(RESET_DEFAULT_SETTINGS);
  plugin->send_configure_change();
  return 1;
}

ChromaKeyUseColorPicker::ChromaKeyUseColorPicker (ChromaKeyAvid * plugin,
						  ChromaKeyWindow * gui,
						  int x, int y)
  : BC_GenericButton (x, y, _("Use color picker"))
{
  this->plugin = plugin;
  this->gui = gui;
}

int
ChromaKeyUseColorPicker::handle_event ()
{
  plugin->config.red = plugin->get_red ();
  plugin->config.green = plugin->get_green ();
  plugin->config.blue = plugin->get_blue ();

  gui->update_sample ();

  plugin->send_configure_change ();
  return 1;
}

ChromaKeyColorThread::ChromaKeyColorThread (ChromaKeyAvid * plugin,
					    ChromaKeyWindow * gui)
  : ColorPicker (0, _("Color"))
{
  this->plugin = plugin;
  this->gui = gui;
}

int
ChromaKeyColorThread::handle_new_color (int output, int alpha)
{
  plugin->config.red   = (float) (output & 0xff0000) / 0xff0000;
  plugin->config.green = (float) (output & 0xff00) / 0xff00;
  plugin->config.blue  = (float) (output & 0xff) / 0xff;

  get_gui()->unlock_window();
  gui->lock_window("ChromaKeyColorThread::handle_new_color");
  gui->update_sample();
  gui->unlock_window();
  get_gui()->lock_window("ChromaKeyColorThread::handle_new_color");

  plugin->send_configure_change ();
  return 1;
}



ChromaKeyServer::ChromaKeyServer (ChromaKeyAvid * plugin)
  : LoadServer (plugin->PluginClient::smp + 1,
		plugin->PluginClient::smp + 1)
// DEBUG
//  : LoadServer (1, 1)
{
  this->plugin = plugin;
}

void
ChromaKeyServer::init_packages ()
{
  for (int i = 0; i < get_total_packages (); i++)
  {
    ChromaKeyPackage *pkg = (ChromaKeyPackage *) get_package (i);
    pkg->y1 = plugin->input->get_h () * i / get_total_packages ();
    pkg->y2 = plugin->input->get_h () * (i + 1) / get_total_packages ();
  }
}

LoadClient *
ChromaKeyServer::new_client ()
{
  return new ChromaKeyUnit (plugin, this);
}

LoadPackage *
ChromaKeyServer::new_package ()
{
  return new ChromaKeyPackage;
}

ChromaKeyPackage::ChromaKeyPackage ():LoadPackage ()
{
}

ChromaKeyUnit::ChromaKeyUnit (ChromaKeyAvid * plugin, ChromaKeyServer * server)
  : LoadClient (server)
{
  this->plugin = plugin;
}



#define ABS(a) (((a) < 0) ? -(a) : (a))



// Compute the same values in the opengl version
#define COMMON_VARIABLES \
    float red = plugin->config.red; \
    float green = plugin->config.green; \
    float blue = plugin->config.blue; \
 \
    float in_slope = plugin->config.in_slope / MAX_SLOPE; \
    float out_slope = plugin->config.out_slope / MAX_SLOPE; \
 \
/* Convert RGB key to HSV key */ \
    float hue_key, saturation_key, value_key; \
    HSV::rgb_to_hsv(red,	\
		green, \
		blue, \
		hue_key, \
		saturation_key, \
		value_key); \
 \
/* hue range */ \
    float tolerance = plugin->config.tolerance * 180 / MAX_VALUE; \
    float tolerance_in = tolerance - in_slope * 180; \
    tolerance_in = MAX(tolerance_in, 0); \
    float tolerance_out = tolerance + out_slope * 180; \
    tolerance_out = MIN(tolerance_out, 180); \
 \
/* distance of wedge point from center */ \
    float sat_distance = plugin->config.saturation_start / MAX_VALUE; \
/* XY shift of input color to get wedge point */ \
    float sat_x = -cos(TO_RAD(hue_key)) * sat_distance; \
    float sat_y = -sin(TO_RAD(hue_key)) * sat_distance; \
/* minimum saturation after wedge point */ \
    float min_s = plugin->config.saturation_line / MAX_VALUE; \
    float min_s_in = min_s + in_slope; \
    float min_s_out = min_s - out_slope; \
 \
    float min_v = plugin->config.min_brightness / MAX_VALUE; \
    float min_v_in = min_v + in_slope; \
    float min_v_out = min_v - out_slope; \
/* ignore min_brightness 0 */ \
    if(plugin->config.min_brightness == 0) min_v_in = 0; \
 \
    float max_v = plugin->config.max_brightness / MAX_VALUE; \
    float max_v_in = max_v - in_slope; \
    float max_v_out = max_v + out_slope; \
/* handle wedge boundaries crossing over */ \
    if(max_v_in < min_v_in) max_v_in = min_v_in = (max_v_in + min_v_in) / 2; \
/* ignore max_brightness 100% */ \
    if(plugin->config.max_brightness == MAX_VALUE) max_v_in = 1.0; \
 \
/* distance of spill wedge point from center */ \
    float spill_distance = sat_distance * (1.0 - plugin->config.spill_saturation / MAX_VALUE); \
/* XY shift of input color to get spill wedge point */ \
    float spill_x = -cos(TO_RAD(hue_key)) * spill_distance; \
    float spill_y = -sin(TO_RAD(hue_key)) * spill_distance; \
/* tolerance of the spill wedge */ \
    float spill_tolerance = tolerance + plugin->config.spill_angle * 90.0 / MAX_VALUE; \
/* borders of the spill wedge */ \
    float min_h = hue_key - spill_tolerance; \
    float max_h = hue_key + spill_tolerance; \
    int desaturate_only = plugin->config.desaturate_only; \
 \
    float alpha_offset = plugin->config.alpha_offset / MAX_VALUE;

// shortest distance between 2 hues
static float hue_distance(float h1, float h2)
{
  float result = h1 - h2;
  if(result < -180) result += 360;
  else if(result > 180) result -= 360;
  return result;
}

// shift H & S based on an X & Y offset
static void shift_hs(float &h_shifted, 
		     float &s_shifted, 
		     float h, 
		     float s, 
		     float x_offset,
		     float y_offset)
{
  float h_rad = TO_RAD(h);
  float x = cos(h_rad) * s;
  float y = sin(h_rad) * s;
  x += x_offset;
  y += y_offset;
  h_shifted = TO_DEG(atan2(y, x));
  s_shifted = hypot(x, y);
}

template <typename component_type> 
void ChromaKeyUnit::process_chromakey(int components, 
				      component_type max, 
				      bool use_yuv, 
				      ChromaKeyPackage *pkg) 
{
  COMMON_VARIABLES

  int w = plugin->input->get_w();

// printf("ChromaKeyUnit::process_chromakey %d hue_key=%f min_h=%f max_h=%f\n", 
// __LINE__, 
// hue_key,
// min_h,
// max_h);
// printf("ChromaKeyUnit::process_chromakey %d tolerance_in=%f tolerance=%f tolerance_out=%f\n", 
// __LINE__, 
// tolerance_in,
// tolerance,
// tolerance_out);
// printf("ChromaKeyUnit::process_chromakey %d min_s_in=%f min_s=%f min_s_out=%f\n", 
// __LINE__, 
// min_s_in,
// min_s,
// min_s_out);
// printf("ChromaKeyUnit::process_chromakey %d min_v_in=%f min_v=%f min_v_out=%f\n", 
// __LINE__, 
// min_v_in,
// min_v,
// min_v_out);
// printf("ChromaKeyUnit::process_chromakey %d max_v_in=%f max_v=%f max_v_out=%f\n", 
// __LINE__, 
// max_v_in,
// max_v,
// max_v_out);

  for (int i = pkg->y1; i < pkg->y2; i++)
  {
    component_type *row = (component_type *) plugin->input->get_rows ()[i];

    for (int j = 0; j < w; j++)
    {
      float r, g, b, a = 1;
      if (!use_yuv)
      {
	r = (float) row[0] / max;
	g = (float) row[1] / max;
	b = (float) row[2] / max;
      }
      else		/* Convert pixel to RGB float */
	YUV::yuv.yuv_to_rgb_f (r, g, b, row[0], row[1], row[2]);

// the input color
      float h, s, v;

// alpha contribution of each component of the HSV space
      float ah = 1, as = 1, av = 1;

      HSV::rgb_to_hsv (r, g, b, h, s, v);

// shift the input color in XY to get the wedge point
      float h_shifted, s_shifted;
      if(!EQUIV(plugin->config.saturation_start, 0))
      {
	shift_hs(h_shifted, 
		 s_shifted, 
		 h, 
		 s, 
		 sat_x,
		 sat_y);
      }
      else
      {
	h_shifted = h;
	s_shifted = s;
      }

/* Get the difference between the shifted input color & the unshifted wedge */
      float h_diff = hue_distance(h_shifted, hue_key);
      h_diff = ABS(h_diff);

// alpha contribution from hue difference
// outside wedge < tolerance_out < tolerance_in < inside wedge < tolerance_in < tolerance_out < outside wedge
      if (tolerance_out > 0)
      {
// completely inside the wedge
	if (h_diff < tolerance_in)
	  ah = 0;
	else
// between the outer & inner slope
	  if(h_diff < tolerance_out)
	    ah = (h_diff - tolerance_in) / (tolerance_out - tolerance_in);
	if(ah > 1) ah = 1;
      }

// alpha contribution from saturation
// outside wedge < min_s_out < min_s_in < inside wedge
      if(s_shifted > min_s_out)
      {
// saturation with offset applied
// completely inside the wedge
	if(s_shifted > min_s_in)
	  as = 0;
// inside the gradient
	if(s_shifted >= min_s_out)
	  as = (min_s_in - s_shifted) / (min_s_in - min_s_out);
      }

// alpha contribution from brightness range
// brightness range is defined by 4 in/out variables
// outside wedge < min_v_out < min_v_in < inside wedge < max_v_in < max_v_out < outside wedge
      if(v > min_v_out)
      {
	if(v < min_v_in || max_v_in >= 1.0)
	  av = (min_v_in - v) / (min_v_in - min_v_out);
	else if(v <= max_v_in)
	  av = 0;
	else if(v <= max_v_out)
	  av = (v - max_v_in) / (max_v_out - max_v_in);
      }

// combine the alpha contribution of every component into a single alpha
      a = MAX(as, ah);
      a = MAX(a, av);

// Spill light processing
      if(spill_tolerance > 0)
      {
// get the difference between the shifted input color to the unshifted spill wedge
	if(!EQUIV(spill_distance, 0))
	{
	  shift_hs(h_shifted, 
		   s_shifted, 
		   h, 
		   s, 
		   spill_x,
		   spill_y);
	}
	else
	{
	  h_shifted = h;
	  s_shifted = s;
	}

// Difference between the shifted hue & the unshifted hue key
	h_diff = hue_distance(h_shifted, hue_key);

// inside the wedge
	if(ABS(h_diff) < spill_tolerance)
	{
	  if(!desaturate_only)
	  {
// the shifted input color in the unshifted wedge
// gives 2 unshifted border colors & the weighting
	    float blend = 0.5 + h_diff / spill_tolerance / 2;
// shift the 2 border colors to the output wedge
	    float min_h_shifted;
	    float min_s_shifted;
	    shift_hs(min_h_shifted, 
		     min_s_shifted, 
		     min_h, 
		     s_shifted, 
		     -spill_x,
		     -spill_y);
	    float max_h_shifted;
	    float max_s_shifted;
	    shift_hs(max_h_shifted, 
		     max_s_shifted, 
		     max_h, 
		     s_shifted, 
		     -spill_x,
		     -spill_y);

// blend the shifted border colors using the unshifted weighting
// the only thing which doesn't restore the key color & doesn't make an edge is
// fading the saturation to 0 in the middle
	    if(blend > 0.5)
	    {
	      h = max_h_shifted;
	      s = max_s_shifted;
    //                        s = max_s_shifted * (blend - 0.5) / 0.5;
    // DEBUG
    //h = 0;
    //s = blend; 
	    }
	    else
	    {
	      h = min_h_shifted;
	      s = min_s_shifted;
    //                        s = min_s_shifted * (0.5 - blend) / 0.5;
    // DEBUG
    //h = 0;
    //s = blend; 
	    }
	  } 
	  else // !desaturate_only
	  {

// fade the saturation to 0 in the middle
	    s *= ABS(h_diff) / spill_tolerance;
	  }

	  if(h < 0) h += 360;
	  HSV::hsv_to_rgb (r, g, b, h, s, v);

// store new color
	  if (!use_yuv)
	  {
	    row[0] = (component_type) ((float) r * max);
	    row[1] = (component_type) ((float) g * max);
	    row[2] = (component_type) ((float) b * max);
	  }
	  else
	    YUV::yuv.rgb_to_yuv_f(r, g, b, row[0], row[1], row[2]);
	}
      }

      a += alpha_offset;
      CLAMP (a, 0.0, 1.0);

      if (plugin->config.show_mask)
      {
	if (use_yuv)
	{
	  row[0] = (component_type) ((float) a * max);
	  row[1] = (component_type) ((float) max / 2);
	  row[2] = (component_type) ((float) max / 2);
	}
	else
	{
	  row[0] = (component_type) ((float) a * max);
	  row[1] = (component_type) ((float) a * max);
	  row[2] = (component_type) ((float) a * max);
	}
      }

      /* Multiply alpha and put back in frame */
      if (components == 4)
      {
	row[3] = MIN ((component_type) (a * max), row[3]);
      }
      else if (use_yuv)
      {
	row[0] = (component_type) ((float) a * row[0]);
	row[1] = (component_type) ((float) a * (row[1] - (max / 2 + 1)) +
				   max / 2 + 1);
	row[2] = (component_type) ((float) a * (row[2] - (max / 2 + 1)) +
				   max / 2 + 1);
      }
      else
      {
	row[0] = (component_type) ((float) a * row[0]);
	row[1] = (component_type) ((float) a * row[1]);
	row[2] = (component_type) ((float) a * row[2]);
      }

      row += components;
    }
  }
}



void ChromaKeyUnit::process_package(LoadPackage *package)
{
  ChromaKeyPackage *pkg = (ChromaKeyPackage*)package;

  switch(plugin->input->get_color_model())
  {
  case BC_RGB_FLOAT:
    process_chromakey<float> (3, 1.0, 0, pkg);
    break;
  case BC_RGBA_FLOAT:
    process_chromakey<float> ( 4, 1.0, 0, pkg);
    break;
  case BC_RGB888:
    process_chromakey<unsigned char> ( 3, 0xff, 0, pkg);
    break;
  case BC_RGBA8888:
    process_chromakey<unsigned char> ( 4, 0xff, 0, pkg);
    break;
  case BC_YUV888:
    process_chromakey<unsigned char> ( 3, 0xff, 1, pkg);
    break;
  case BC_YUVA8888:
    process_chromakey<unsigned char> ( 4, 0xff, 1, pkg);
    break;
  case BC_YUV161616:
    process_chromakey<uint16_t> (3, 0xffff, 1, pkg);
    break;
  case BC_YUVA16161616:
    process_chromakey<uint16_t> (4, 0xffff, 1, pkg);
    break;
  }
}



REGISTER_PLUGIN(ChromaKeyAvid)



ChromaKeyAvid::ChromaKeyAvid(PluginServer *server)
  : PluginVClient(server)
{
  engine = 0;
}

ChromaKeyAvid::~ChromaKeyAvid()
{
  if(engine) delete engine;
}


int ChromaKeyAvid::process_buffer(VFrame *frame,
				  int64_t start_position,
				  double frame_rate)
{
  load_configuration();
  this->input = frame;
  this->output = frame;

  read_frame(frame,
	     0,
	     start_position,
	     frame_rate,
	     get_use_opengl());
  if(get_use_opengl()) return run_opengl();

  if(!engine) engine = new ChromaKeyServer(this);
  engine->process_packages();

  return 0;
}

const char* ChromaKeyAvid::plugin_title() { return N_("Chroma key (Avid)"); }
int ChromaKeyAvid::is_realtime() { return 1; }


LOAD_CONFIGURATION_MACRO(ChromaKeyAvid, ChromaKeyConfig)


void ChromaKeyAvid::save_data(KeyFrame * keyframe)
{
  FileXML output;
  output.set_shared_output(keyframe->xbuf);
  output.tag.set_title("CHROMAKEY_AVID");
  output.tag.set_property("RED", config.red);
  output.tag.set_property("GREEN", config.green);
  output.tag.set_property("BLUE", config.blue);
  output.tag.set_property("MIN_BRIGHTNESS", config.min_brightness);
  output.tag.set_property("MAX_BRIGHTNESS", config.max_brightness);
  output.tag.set_property("SATURATION_START", config.saturation_start);
  output.tag.set_property("SATURATION_LINE", config.saturation_line);
  output.tag.set_property("TOLERANCE", config.tolerance);
  output.tag.set_property("IN_SLOPE", config.in_slope);
  output.tag.set_property("OUT_SLOPE", config.out_slope);
  output.tag.set_property("ALPHA_OFFSET", config.alpha_offset);
  output.tag.set_property("SPILL_SATURATION", config.spill_saturation);
  output.tag.set_property("SPILL_ANGLE", config.spill_angle);
  output.tag.set_property("DESATURATE_ONLY", config.desaturate_only);
  output.tag.set_property("SHOW_MASK", config.show_mask);
  output.append_tag();
  output.tag.set_title("/CHROMAKEY_AVID");
  output.append_tag();
  output.append_newline();
  output.terminate_string();
}

void ChromaKeyAvid::read_data(KeyFrame * keyframe)
{
  FileXML input;

  input.set_shared_input(keyframe->xbuf);

  while( !input.read_tag() )
  {
    if( input.tag.title_is("CHROMAKEY_AVID") )
    {
      config.red   = input.tag.get_property("RED",   config.red);
      config.green = input.tag.get_property("GREEN", config.green);
      config.blue  = input.tag.get_property("BLUE",  config.blue);
      config.min_brightness =
	input.tag.get_property("MIN_BRIGHTNESS", config.min_brightness);
      config.max_brightness =
	input.tag.get_property("MAX_BRIGHTNESS", config.max_brightness);
      config.saturation_start =
	input.tag.get_property("SATURATION_START", config.saturation_start);
      config.saturation_line =
	input.tag.get_property("SATURATION_LINE", config.saturation_line);
      config.tolerance =
	input.tag.get_property("TOLERANCE", config.tolerance);
      config.in_slope =
	input.tag.get_property("IN_SLOPE", config.in_slope);
      config.out_slope =
	input.tag.get_property("OUT_SLOPE", config.out_slope);
      config.alpha_offset =
	input.tag.get_property("ALPHA_OFFSET", config.alpha_offset);
      config.spill_saturation =
	input.tag.get_property("SPILL_SATURATION", config.spill_saturation);
      config.spill_angle =
	input.tag.get_property("SPILL_ANGLE", config.spill_angle);
      config.desaturate_only =
	input.tag.get_property("DESATURATE_ONLY", config.desaturate_only);
      config.show_mask =
	input.tag.get_property("SHOW_MASK", config.show_mask);

      config.undo_red              = config.red;
      config.undo_green            = config.green;
      config.undo_blue             = config.blue;
      config.undo_min_brightness   = config.min_brightness;
      config.undo_max_brightness   = config.max_brightness;
      config.undo_tolerance        = config.tolerance;
      config.undo_saturation_start = config.saturation_start;
      config.undo_saturation_line  = config.saturation_line;
      config.undo_in_slope         = config.in_slope;
      config.undo_out_slope        = config.out_slope;
      config.undo_alpha_offset     = config.alpha_offset;
      config.undo_spill_saturation = config.spill_saturation;
      config.undo_spill_angle      = config.spill_angle;
      config.undo_desaturate_only  = config.desaturate_only;
      config.undo_show_mask        = config.show_mask;
    }
  }
}


NEW_WINDOW_MACRO(ChromaKeyAvid, ChromaKeyWindow)


void ChromaKeyAvid::update_gui()
{
  if( thread )
  {
    load_configuration();
    ChromaKeyWindow *window = (ChromaKeyWindow*)thread->window;
    window->lock_window();
    window->update_gui(RESET_ALL);
    window->unlock_window();
  }
}

void ChromaKeyWindow::update_gui(int clear)
{
  ChromaKeyConfig &config = plugin->config;
  switch(clear)
  {
  case RESET_RGB:
    update_sample();
    break;
  case RESET_MIN_BRIGHTNESS:
    min_brightness->update(config.min_brightness);
    min_brightness_text->update(config.min_brightness);
    break;
  case RESET_MAX_BRIGHTNESS:
    max_brightness->update(config.max_brightness);
    max_brightness_text->update(config.max_brightness);
    break;
  case RESET_TOLERANCE:
    tolerance->update(config.tolerance);
    tolerance_text->update(config.tolerance);
    break;
  case RESET_SATURATION_START:
    saturation_start->update(config.saturation_start);
    saturation_start_text->update(config.saturation_start);
    break;
  case RESET_SATURATION_LINE:
    saturation_line->update(config.saturation_line);
    saturation_line_text->update(config.saturation_line);
    break;
  case RESET_IN_SLOPE:
    in_slope->update(config.in_slope);
    in_slope_text->update(config.in_slope);
    break;
  case RESET_OUT_SLOPE:
    out_slope->update(config.out_slope);
    out_slope_text->update(config.out_slope);
    break;
  case RESET_ALPHA_OFFSET:
    alpha_offset->update(config.alpha_offset);
    alpha_offset_text->update(config.alpha_offset);
    break;
  case RESET_SPILL_SATURATION:
    spill_saturation->update(config.spill_saturation);
    spill_saturation_text->update(config.spill_saturation);
    break;
  case RESET_SPILL_ANGLE:
    spill_angle->update(config.spill_angle);
    spill_angle_text->update(config.spill_angle);
    break;
  case RESET_ALL:
  case RESET_DEFAULT_SETTINGS:
  default:
    min_brightness->update(config.min_brightness);
    min_brightness_text->update(config.min_brightness);
    max_brightness->update(config.max_brightness);
    max_brightness_text->update(config.max_brightness);
    saturation_start->update(config.saturation_start);
    saturation_start_text->update(config.saturation_start);
    saturation_line->update(config.saturation_line);
    saturation_line_text->update(config.saturation_line);
    tolerance->update(config.tolerance);
    tolerance_text->update(config.tolerance);
    in_slope->update(config.in_slope);
    in_slope_text->update(config.in_slope);
    out_slope->update(config.out_slope);
    out_slope_text->update(config.out_slope);
    alpha_offset->update(config.alpha_offset);
    alpha_offset_text->update(config.alpha_offset);
    spill_saturation->update(config.spill_saturation);
    spill_saturation_text->update(config.spill_saturation);
    spill_angle->update(config.spill_angle);
    spill_angle_text->update(config.spill_angle);
    desaturate_only->update(config.desaturate_only);
    show_mask->update(config.show_mask);
    update_sample();
    break;
  }
}



int ChromaKeyAvid::handle_opengl()
{
#ifdef HAVE_GL
// For macro
	ChromaKeyAvid *plugin = this;
	COMMON_VARIABLES

	static const char *yuv_shader =
		"const vec3 black = vec3(0.0, 0.5, 0.5);\n"
		"uniform mat3 yuv_to_rgb_matrix;\n"
		"uniform mat3 rgb_to_yuv_matrix;\n"
		"uniform float yminf;\n"
		"\n"
		"vec4 yuv_to_rgb(vec4 color)\n"
		"{\n"
		"	color.rgb -= vec3(yminf, 0.5, 0.5);\n"
		"	color.rgb = yuv_to_rgb_matrix * color.rgb;\n"
		"	return color;\n"
		"}\n"
		"\n"
		"vec4 rgb_to_yuv(vec4 color)\n"
		"{\n"
		"	color.rgb = rgb_to_yuv_matrix * color.rgb;\n"
		"	color.rgb += vec3(yminf, 0.5, 0.5);\n"
		"	return color;\n"
		"}\n";

	static const char *rgb_shader =
		"const vec3 black = vec3(0.0, 0.0, 0.0);\n"
		"\n"
		"vec4 yuv_to_rgb(vec4 color)\n"
		"{\n"
		"	return color;\n"
		"}\n"
		"vec4 rgb_to_yuv(vec4 color)\n"
		"{\n"
		"	return color;\n"
		"}\n";

	static const char *hsv_shader =
		"vec4 rgb_to_hsv(vec4 color)\n"
		"{\n"
			RGB_TO_HSV_FRAG("color")
		"	return color;\n"
		"}\n"
		"\n"
		"vec4 hsv_to_rgb(vec4 color)\n"
		"{\n"
			HSV_TO_RGB_FRAG("color")
		"	return color;\n"
		"}\n"
		"\n";

	static const char *show_rgbmask_shader =
		"vec4 show_mask(vec4 color, vec4 color2)\n"
		"{\n"
		"	return vec4(1.0, 1.0, 1.0, min(color.a, color2.a));"
		"}\n";

	static const char *show_yuvmask_shader =
		"vec4 show_mask(vec4 color, vec4 color2)\n"
		"{\n"
		"	return vec4(1.0, 0.5, 0.5, min(color.a, color2.a));"
		"}\n";

	static const char *nomask_shader =
		"vec4 show_mask(vec4 color, vec4 color2)\n"
		"{\n"
		"	return vec4(color.rgb, min(color.a, color2.a));"
		"}\n";

	get_output()->to_texture();
	get_output()->enable_opengl();
	get_output()->init_screen();

        const char *shader_stack[16];
        memset(shader_stack,0, sizeof(shader_stack));
        int current_shader = 0;

	shader_stack[current_shader++] = \
		 !BC_CModels::is_yuv(get_output()->get_color_model()) ?
			rgb_shader : yuv_shader;
	shader_stack[current_shader++] = hsv_shader;
	shader_stack[current_shader++] = !config.show_mask ? nomask_shader :
		 !BC_CModels::is_yuv(get_output()->get_color_model()) ?
			show_rgbmask_shader : show_yuvmask_shader ;
	extern unsigned char _binary_chromakeyavid_sl_start[];
	static const char *shader_frag = (char*)_binary_chromakeyavid_sl_start;
	shader_stack[current_shader++] = shader_frag;

	shader_stack[current_shader] = 0;
	unsigned int shader = VFrame::make_shader(shader_stack);
	if( shader > 0 ) {
		glUseProgram(shader);
		glUniform1i(glGetUniformLocation(shader, "tex"), 0);
		glUniform1f(glGetUniformLocation(shader, "red"), red);
		glUniform1f(glGetUniformLocation(shader, "green"), green);
		glUniform1f(glGetUniformLocation(shader, "blue"), blue);
		glUniform1f(glGetUniformLocation(shader, "in_slope"), in_slope);
		glUniform1f(glGetUniformLocation(shader, "out_slope"), out_slope);
		glUniform1f(glGetUniformLocation(shader, "tolerance"), tolerance);
		glUniform1f(glGetUniformLocation(shader, "tolerance_in"), tolerance_in);
		glUniform1f(glGetUniformLocation(shader, "tolerance_out"), tolerance_out);
		glUniform1f(glGetUniformLocation(shader, "sat_x"), sat_x);
		glUniform1f(glGetUniformLocation(shader, "sat_y"), sat_y);
		glUniform1f(glGetUniformLocation(shader, "min_s"), min_s);
		glUniform1f(glGetUniformLocation(shader, "min_s_in"), min_s_in);
		glUniform1f(glGetUniformLocation(shader, "min_s_out"), min_s_out);
		glUniform1f(glGetUniformLocation(shader, "min_v"), min_v);
		glUniform1f(glGetUniformLocation(shader, "min_v_in"), min_v_in);
		glUniform1f(glGetUniformLocation(shader, "min_v_out"), min_v_out);
		glUniform1f(glGetUniformLocation(shader, "max_v"), max_v);
		glUniform1f(glGetUniformLocation(shader, "max_v_in"), max_v_in);
		glUniform1f(glGetUniformLocation(shader, "max_v_out"), max_v_out);
		glUniform1f(glGetUniformLocation(shader, "spill_distance"), spill_distance);
		glUniform1f(glGetUniformLocation(shader, "spill_x"), spill_x);
		glUniform1f(glGetUniformLocation(shader, "spill_y"), spill_y);
		glUniform1f(glGetUniformLocation(shader, "spill_tolerance"), spill_tolerance);
		glUniform1f(glGetUniformLocation(shader, "min_h"), min_h);
		glUniform1f(glGetUniformLocation(shader, "max_h"), max_h);
		glUniform1i(glGetUniformLocation(shader, "desaturate_only"), desaturate_only);
		glUniform1f(glGetUniformLocation(shader, "alpha_offset"), alpha_offset);
		glUniform1f(glGetUniformLocation(shader, "hue_key"), hue_key);
		glUniform1f(glGetUniformLocation(shader, "saturation_key"), saturation_key);
		glUniform1f(glGetUniformLocation(shader, "value_key"), value_key);
		if( BC_CModels::is_yuv(get_output()->get_color_model()) )
			BC_GL_COLORS(shader);
	}

	get_output()->bind_texture(0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	if(BC_CModels::components(get_output()->get_color_model()) == 3)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		get_output()->clear_pbuffer();
	}
	get_output()->draw_texture();

	glUseProgram(0);
	get_output()->set_opengl_state(VFrame::SCREEN);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glDisable(GL_BLEND);


#endif
	return 0;
}
