
/*
 * CINELERRA
 * Copyright (C) 2012 Adam Williams <broadcast at earthling dot net>
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



ChromaKeyConfig::ChromaKeyConfig ()
{
	reset(RESET_DEFAULT_SETTINGS);
}

void ChromaKeyConfig::reset(int clear)
{
	switch(clear) {
		case RESET_RGB :
			red = 0.0;
			green = 1.0;
			blue = 0.0;
			break;
		case RESET_MIN_BRIGHTNESS :
			min_brightness = 50.0;
			break;
		case RESET_MAX_BRIGHTNESS :
			max_brightness = 100.0;
			break;
		case RESET_TOLERANCE :
			tolerance = 15.0;
			break;
		case RESET_SATURATION :
			saturation = 0.0;
			break;
		case RESET_MIN_SATURATION :
			min_saturation = 50.0;
			break;
		case RESET_IN_SLOPE :
			in_slope = 2;
			break;
		case RESET_OUT_SLOPE :
			out_slope = 2;
			break;
		case RESET_ALPHA_OFFSET :
			alpha_offset = 0;
			break;
		case RESET_SPILL_THRESHOLD :
			spill_threshold = 0.0;
			break;
		case RESET_SPILL_AMOUNT :
			spill_amount = 90.0;
			break;
		case RESET_ALL :
		case RESET_DEFAULT_SETTINGS :
		default:
			red = 0.0;
			green = 1.0;
			blue = 0.0;
			min_brightness = 50.0;
			max_brightness = 100.0;
			tolerance = 15.0;
			saturation = 0.0;
			min_saturation = 50.0;

			in_slope = 2;
			out_slope = 2;
			alpha_offset = 0;

			spill_threshold = 0.0;
			spill_amount = 90.0;

			show_mask = 0;
			break;
	}
}


void ChromaKeyConfig::copy_from (ChromaKeyConfig & src)
{
  red = src.red;
  green = src.green;
  blue = src.blue;
  spill_threshold = src.spill_threshold;
  spill_amount = src.spill_amount;
  min_brightness = src.min_brightness;
  max_brightness = src.max_brightness;
  saturation = src.saturation;
  min_saturation = src.min_saturation;
  tolerance = src.tolerance;
  in_slope = src.in_slope;
  out_slope = src.out_slope;
  alpha_offset = src.alpha_offset;
  show_mask = src.show_mask;
}

int ChromaKeyConfig::equivalent (ChromaKeyConfig & src)
{
  return (EQUIV (red, src.red) &&
	  EQUIV (green, src.green) &&
	  EQUIV (blue, src.blue) &&
	  EQUIV (spill_threshold, src.spill_threshold) &&
	  EQUIV (spill_amount, src.spill_amount) &&
	  EQUIV (min_brightness, src.min_brightness) &&
	  EQUIV (max_brightness, src.max_brightness) &&
	  EQUIV (saturation, src.saturation) &&
	  EQUIV (min_saturation, src.min_saturation) &&
	  EQUIV (tolerance, src.tolerance) &&
	  EQUIV (in_slope, src.in_slope) &&
	  EQUIV (out_slope, src.out_slope) &&
	  EQUIV (show_mask, src.show_mask) &&
	  EQUIV (alpha_offset, src.alpha_offset));
}

void ChromaKeyConfig::interpolate (ChromaKeyConfig & prev,
			      ChromaKeyConfig & next,
			      int64_t prev_frame,
			      int64_t next_frame, int64_t current_frame)
{
  double next_scale =
    (double) (current_frame - prev_frame) / (next_frame - prev_frame);
  double prev_scale =
    (double) (next_frame - current_frame) / (next_frame - prev_frame);

  this->red = prev.red * prev_scale + next.red * next_scale;
  this->green = prev.green * prev_scale + next.green * next_scale;
  this->blue = prev.blue * prev_scale + next.blue * next_scale;
  this->spill_threshold =
    prev.spill_threshold * prev_scale + next.spill_threshold * next_scale;
  this->spill_amount =
    prev.spill_amount * prev_scale + next.tolerance * next_scale;
  this->min_brightness =
    prev.min_brightness * prev_scale + next.min_brightness * next_scale;
  this->max_brightness =
    prev.max_brightness * prev_scale + next.max_brightness * next_scale;
  this->saturation =
    prev.saturation * prev_scale + next.saturation * next_scale;
  this->min_saturation =
    prev.min_saturation * prev_scale + next.min_saturation * next_scale;
  this->tolerance = prev.tolerance * prev_scale + next.tolerance * next_scale;
  this->in_slope = prev.in_slope * prev_scale + next.in_slope * next_scale;
  this->out_slope = prev.out_slope * prev_scale + next.out_slope * next_scale;
  this->alpha_offset =
    prev.alpha_offset * prev_scale + next.alpha_offset * next_scale;
  this->show_mask = next.show_mask;

}

int ChromaKeyConfig::get_color ()
{
  int red = (int) (CLIP (this->red, 0, 1) * 0xff);
  int green = (int) (CLIP (this->green, 0, 1) * 0xff);
  int blue = (int) (CLIP (this->blue, 0, 1) * 0xff);
  return (red << 16) | (green << 8) | blue;
}



ChromaKeyWindow::ChromaKeyWindow (ChromaKeyHSV * plugin)
 : PluginClientWindow(plugin,
	   xS(500),
	   yS(550),
	   xS(500),
	   yS(550),
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
	int xs10 = xS(10), xs20 = xS(20), xs100 = xS(100), xs200 = xS(200);
	int ys10 = yS(10), ys20 = yS(20), ys30 = yS(30), ys40 = yS(40), ys50 = yS(50);
	int y = ys10,  x2 = xS(160), x3 = xS(260);
	int x = xs10;
	int clr_x = get_w()-x - xS(22); // note: clrBtn_w = 22

	BC_Title *title;
	BC_TitleBar *title_bar;
	BC_Bar *bar;

// Color section
	add_subwindow(title_bar = new BC_TitleBar(x, y, get_w()-2*x, xs20, xs10, _("Color")));
	y += ys20;

	add_subwindow (color = new ChromaKeyColor (plugin, this, x, y));
	// Info for Sample rectangle:       x_slider w_slider w_sample
	//                                        \       |      /    y,   w,     h
	add_subwindow (sample = new BC_SubWindow (x3 + xs200 - xs100, y, xs100, ys50));
	y += ys30;
	add_subwindow (use_colorpicker = new ChromaKeyUseColorPicker (plugin, this, x, y));
	y += ys30;
	add_subwindow (show_mask = new ChromaKeyShowMask (plugin, x, y));

// Key parameters section
	y += ys30;
	add_subwindow(title_bar = new BC_TitleBar(x, y, get_w()-2*x, xs20, xs10, _("Key parameters")));
	y += ys20;
	add_subwindow (title = new BC_Title (x, y, _("Hue Tolerance:")));
	tolerance_text = new ChromaKeyFText(plugin, this,
		0, &(plugin->config.tolerance), (x + x2), y, MIN_VALUE, MAX_VALUE);
	tolerance_text->create_objects();
	tolerance_slider = new ChromaKeyFSlider(plugin,
		tolerance_text, &(plugin->config.tolerance), x3, y, MIN_VALUE, MAX_VALUE, xs200);
	add_subwindow(tolerance_slider);
	tolerance_text->slider = tolerance_slider;
	clr_x = x3 + tolerance_slider->get_w() + x;
	add_subwindow(tolerance_Clr = new ChromaKeyClr(plugin, this, clr_x, y, RESET_TOLERANCE));
	y += ys30;

	add_subwindow (title = new BC_Title (x, y, _("Min. Brightness:")));
	min_brightness_text = new ChromaKeyFText(plugin, this,
		0, &(plugin->config.min_brightness), (x + x2), y,
		MIN_VALUE, MAX_VALUE);
	min_brightness_text->create_objects();
	min_brightness_slider = new ChromaKeyFSlider(plugin,
		min_brightness_text, &(plugin->config.min_brightness), x3, y, MIN_VALUE, MAX_VALUE, xs200);
	add_subwindow(min_brightness_slider);
	min_brightness_text->slider = min_brightness_slider;
	add_subwindow(min_brightness_Clr = new ChromaKeyClr(plugin, this, clr_x, y, RESET_MIN_BRIGHTNESS));
	y += ys30;

	add_subwindow (title = new BC_Title (x, y, _("Max. Brightness:")));
	max_brightness_text = new ChromaKeyFText(plugin, this,
		0, &(plugin->config.max_brightness), (x + x2), y, MIN_VALUE, MAX_VALUE);
	max_brightness_text->create_objects();
	max_brightness_slider = new ChromaKeyFSlider(plugin,
		max_brightness_text, &(plugin->config.max_brightness), x3, y, MIN_VALUE, MAX_VALUE, xs200);
	add_subwindow(max_brightness_slider);
	max_brightness_text->slider = max_brightness_slider;
	add_subwindow(max_brightness_Clr = new ChromaKeyClr(plugin, this, clr_x, y, RESET_MAX_BRIGHTNESS));
	y += ys30;

	add_subwindow (title = new BC_Title (x, y, _("Saturation Offset:")));
	saturation_text = new ChromaKeyFText(plugin, this,
		0, &(plugin->config.saturation), (x + x2), y, MIN_VALUE, MAX_VALUE);
	saturation_text->create_objects();
	saturation_slider = new ChromaKeyFSlider(plugin,
		saturation_text, &(plugin->config.saturation), x3, y, MIN_VALUE, MAX_VALUE, xs200);
	add_subwindow(saturation_slider);
	saturation_text->slider = saturation_slider;
	add_subwindow(saturation_Clr = new ChromaKeyClr(plugin, this, clr_x, y, RESET_SATURATION));
	y += ys30;

	add_subwindow (title = new BC_Title (x, y, _("Min Saturation:")));
	min_saturation_text = new ChromaKeyFText(plugin, this,
		0, &(plugin->config.min_saturation), (x + x2), y, MIN_VALUE, MAX_VALUE);
	min_saturation_text->create_objects();
	min_saturation_slider = new ChromaKeyFSlider(plugin,
		min_saturation_text, &(plugin->config.min_saturation), x3, y, MIN_VALUE, MAX_VALUE, xs200);
	add_subwindow(min_saturation_slider);
	min_saturation_text->slider = min_saturation_slider;
	add_subwindow(min_saturation_Clr = new ChromaKeyClr(plugin, this, clr_x, y, RESET_MIN_SATURATION));
	y += ys40;

// Mask tweaking section
	add_subwindow(title_bar = new BC_TitleBar(x, y, get_w()-2*x, xs20, xs10, _("Mask tweaking")));
	y += ys20;
	add_subwindow (title = new BC_Title (x, y, _("In Slope:")));
	in_slope_text = new ChromaKeyFText(plugin, this,
		0, &(plugin->config.in_slope), (x + x2), y, MIN_SLOPE, MAX_SLOPE);
	in_slope_text->create_objects();
	in_slope_slider = new ChromaKeyFSlider(plugin,
		in_slope_text, &(plugin->config.in_slope), x3, y, MIN_SLOPE, MAX_SLOPE, xs200);
	add_subwindow(in_slope_slider);
	in_slope_text->slider = in_slope_slider;
	add_subwindow(in_slope_Clr = new ChromaKeyClr(plugin, this, clr_x, y, RESET_IN_SLOPE));
	y += ys30;

	add_subwindow (title = new BC_Title (x, y, _("Out Slope:")));
	out_slope_text = new ChromaKeyFText(plugin, this,
		0, &(plugin->config.out_slope), (x + x2), y, MIN_SLOPE, MAX_SLOPE);
	out_slope_text->create_objects();
	out_slope_slider = new ChromaKeyFSlider(plugin,
		out_slope_text, &(plugin->config.out_slope), x3, y, MIN_SLOPE, MAX_SLOPE, xs200);
	add_subwindow(out_slope_slider);
	out_slope_text->slider = out_slope_slider;
	add_subwindow(out_slope_Clr = new ChromaKeyClr(plugin, this, clr_x, y, RESET_OUT_SLOPE));
	y += ys30;

	add_subwindow (title = new BC_Title (x, y, _("Alpha Offset:")));
	alpha_offset_text = new ChromaKeyFText(plugin, this,
		0, &(plugin->config.alpha_offset), (x + x2), y, -MAX_ALPHA, MAX_ALPHA);
	alpha_offset_text->create_objects();
	alpha_offset_slider = new ChromaKeyFSlider(plugin,
		alpha_offset_text, &(plugin->config.alpha_offset), x3, y, -MAX_ALPHA, MAX_ALPHA, xs200);
	add_subwindow(alpha_offset_slider);
	alpha_offset_text->slider = alpha_offset_slider;
	add_subwindow(alpha_offset_Clr = new ChromaKeyClr(plugin, this, clr_x, y, RESET_ALPHA_OFFSET));
	y += ys40;

// Spill light control section
	add_subwindow(title_bar = new BC_TitleBar(x, y, get_w()-2*x, xs20, xs10, _("Spill light control")));
	y += ys20;
	add_subwindow (title = new BC_Title (x, y, _("Spill Threshold:")));
	spill_threshold_text = new ChromaKeyFText(plugin, this,
		0, &(plugin->config.spill_threshold), (x + x2), y, MIN_VALUE, MAX_VALUE);
	spill_threshold_text->create_objects();
	spill_threshold_slider = new ChromaKeyFSlider(plugin,
		spill_threshold_text, &(plugin->config.spill_threshold), x3, y, MIN_VALUE, MAX_VALUE, xs200);
	add_subwindow(spill_threshold_slider);
	spill_threshold_text->slider = spill_threshold_slider;
	add_subwindow(spill_threshold_Clr = new ChromaKeyClr(plugin, this, clr_x, y, RESET_SPILL_THRESHOLD));
	y += ys30;

	add_subwindow (title = new BC_Title (x, y, _("Spill Compensation:")));
	spill_amount_text = new ChromaKeyFText(plugin, this,
		0, &(plugin->config.spill_amount), (x + x2), y, MIN_VALUE, MAX_VALUE);
	spill_amount_text->create_objects();
	spill_amount_slider = new ChromaKeyFSlider(plugin,
		spill_amount_text, &(plugin->config.spill_amount), x3, y, MIN_VALUE, MAX_VALUE, xs200);
	add_subwindow(spill_amount_slider);
	spill_amount_text->slider = spill_amount_slider;
	add_subwindow(spill_amount_Clr = new ChromaKeyClr(plugin, this, clr_x, y, RESET_SPILL_AMOUNT));
	y += ys40;

// Reset section
	add_subwindow(bar = new BC_Bar(x, y, get_w()-2*x));
	y += ys10;
	add_subwindow(reset = new ChromaKeyReset(plugin, this, x, y));

	color_thread = new ChromaKeyColorThread (plugin, this);

	update_sample();
	show_window();
}

void ChromaKeyWindow::update_sample()
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


ChromaKeyColor::ChromaKeyColor (ChromaKeyHSV * plugin,
				  ChromaKeyWindow * gui, int x, int y):
BC_GenericButton (x, y, _("Color..."))
{
  this->plugin = plugin;
  this->gui = gui;
}

int ChromaKeyColor::handle_event ()
{
  gui->color_thread->start_window (plugin->config.get_color (), 0xff);
  return 1;
}



ChromaKeyFText::ChromaKeyFText(ChromaKeyHSV *plugin, ChromaKeyWindow *gui,
	ChromaKeyFSlider *slider, float *output, int x, int y, float min, float max)
 : BC_TumbleTextBox(gui, *output,
	min, max, x, y, xS(60), 2)
{
	this->plugin = plugin;
	this->gui = gui;
	this->output = output;
	this->slider = slider;
	this->min = min;
	this->max = max;
	set_increment(0.01);
}

ChromaKeyFText::~ChromaKeyFText()
{
}

int ChromaKeyFText::handle_event()
{
	*output = atof(get_text());
	if(*output > max) *output = max;
	else if(*output < min) *output = min;
	slider->update(*output);
	plugin->send_configure_change();
	return 1;
}

ChromaKeyFSlider::ChromaKeyFSlider(ChromaKeyHSV *plugin,
	ChromaKeyFText *text, float *output, int x, int y,
	float min, float max, int w)
 : BC_FSlider(x, y, 0, w, w, min, max, *output)
{
	this->plugin = plugin;
	this->output = output;
	this->text = text;
	set_precision (0.01);
	enable_show_value(0); // Hide caption
}

ChromaKeyFSlider::~ChromaKeyFSlider()
{
}

int ChromaKeyFSlider::handle_event()
{
	*output = get_value();
	text->update(*output);
	plugin->send_configure_change();
	return 1;
}

ChromaKeyClr::ChromaKeyClr(ChromaKeyHSV *plugin, ChromaKeyWindow *gui, int x, int y, int clear)
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




ChromaKeyShowMask::ChromaKeyShowMask (ChromaKeyHSV * plugin, int x, int y):BC_CheckBox (x, y, plugin->config.show_mask,
	     _
	     ("Show Mask"))
{
  this->plugin = plugin;

}

int ChromaKeyShowMask::handle_event ()
{
  plugin->config.show_mask = get_value ();
  plugin->send_configure_change ();
  return 1;
}

ChromaKeyReset::ChromaKeyReset (ChromaKeyHSV *plugin, ChromaKeyWindow *gui, int x, int y)
 :BC_GenericButton(x, y, _("Reset"))
{
  this->plugin = plugin;
  this->gui = gui;
}

int ChromaKeyReset::handle_event()
{
	plugin->config.reset(RESET_DEFAULT_SETTINGS);
	gui->update_gui(RESET_DEFAULT_SETTINGS);
	plugin->send_configure_change();
	return 1;
}

ChromaKeyUseColorPicker::ChromaKeyUseColorPicker (ChromaKeyHSV * plugin, ChromaKeyWindow * gui, int x, int y)
 : BC_GenericButton (x, y,
		  _
		  ("Use color picker"))
{
  this->plugin = plugin;
  this->gui = gui;
}

int ChromaKeyUseColorPicker::handle_event ()
{
  plugin->config.red = plugin->get_red ();
  plugin->config.green = plugin->get_green ();
  plugin->config.blue = plugin->get_blue ();

  gui->update_sample ();


  plugin->send_configure_change ();
  return 1;
}






ChromaKeyColorThread::ChromaKeyColorThread (ChromaKeyHSV * plugin, ChromaKeyWindow * gui)
 : ColorPicker (1, _("Inner color"))
{
  this->plugin = plugin;
  this->gui = gui;
}

int ChromaKeyColorThread::handle_new_color (int output, int alpha)
{
  plugin->config.red = (float) (output & 0xff0000) / 0xff0000;
  plugin->config.green = (float) (output & 0xff00) / 0xff00;
  plugin->config.blue = (float) (output & 0xff) / 0xff;

  get_gui()->unlock_window();
  gui->lock_window("ChromaKeyColorThread::handle_new_color");
  gui->update_sample ();
  gui->unlock_window();
  get_gui()->lock_window("ChromaKeyColorThread::handle_new_color");


  plugin->send_configure_change ();
  return 1;
}








ChromaKeyServer::ChromaKeyServer (ChromaKeyHSV * plugin):LoadServer (plugin->PluginClient::smp + 1,
	    plugin->PluginClient::smp +
	    1)
{
  this->plugin = plugin;
}

void ChromaKeyServer::init_packages ()
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

ChromaKeyUnit::ChromaKeyUnit (ChromaKeyHSV * plugin, ChromaKeyServer * server):LoadClient
  (server)
{
  this->plugin = plugin;
}




#define ABS(a) ((a<0)?-(a):a)
// Reuse as much as possible in the opengl version
#define OUTER_VARIABLES \
  float red = plugin->config.red; \
  float green = plugin->config.green; \
  float blue = plugin->config.blue; \
 \
  float in_slope = plugin->config.in_slope / 100; \
  float out_slope = plugin->config.out_slope / 100; \
 \
  float tolerance = plugin->config.tolerance / 100; \
  float tolerance_in = tolerance - in_slope; \
  float tolerance_out = tolerance + out_slope; \
 \
  float sat = plugin->config.saturation / 100; \
  float min_s = plugin->config.min_saturation / 100; \
  float min_s_in = min_s + in_slope; \
  float min_s_out = min_s - out_slope; \
 \
  float min_v = plugin->config.min_brightness / 100; \
  float min_v_in = min_v + in_slope; \
  float min_v_out = min_v - out_slope; \
 \
  float max_v = plugin->config.max_brightness / 100; \
  float max_v_in = max_v - in_slope; \
  float max_v_out = max_v + out_slope; \
 \
  float spill_threshold = plugin->config.spill_threshold / 100; \
  float spill_amount = 1.0 - plugin->config.spill_amount / 100; \
 \
  float alpha_offset = plugin->config.alpha_offset / 100; \
 \
/* Convert RGB key to HSV key */ \
	float hue_key, saturation_key, value_key; \
	HSV::rgb_to_hsv(red,  \
		green, \
		blue, \
		hue_key, \
		saturation_key, \
		value_key);


template <typename component_type>
void ChromaKeyUnit::process_chromakey(int components,
	component_type max,
	bool use_yuv,
	ChromaKeyPackage *pkg)
{ 
	OUTER_VARIABLES

	int w = plugin->input->get_w();

	for (int i = pkg->y1; i < pkg->y2; i++)
	{
    	component_type *row = (component_type *) plugin->input->get_rows ()[i];

    	for (int j = 0; j < w; j++)
		{
			float r, g, b, a = 1;
			if (!use_yuv) {
				r = (float) row[0] / max;
				g = (float) row[1] / max;
				b = (float) row[2] / max;
			}
			else
	    		YUV::yuv.yuv_to_rgb_f (r, g, b, row[0], row[1], row[2]);

			float h, s, v;

			float av = 1, ah = 1, as = 1, avm = 1;
			bool has_match = true;

	  		HSV::rgb_to_hsv (r, g, b, h, s, v);

// First, test if the hue is in range

/* Hue wrap */
			if(h <= hue_key - tolerance_in * 180.0)
				h += 360.0;
			else
			if(h >= hue_key + tolerance_in * 180.0)
				h -= 360.0;


			if (tolerance == 0)
	    	    ah = 1.0;
			else
			if (ABS (h - hue_key) < tolerance_in * 180)
	    	    ah = 0;
			else
			if ((out_slope != 0) && (ABS (h - hue_key) < tolerance * 180))
/* we scale alpha between 0 and 1/2 */
	    	    ah = ABS (h - hue_key) / tolerance / 360;
			else
			if (ABS (h - hue_key) < tolerance_out * 180)
/* we scale alpha between 1/2 and 1 */
	    	    ah = ABS (h - hue_key) / tolerance_out / 360;
			else
	    	    has_match = false;

// Check if the saturation matches
			if (has_match)
	    	{
	    		if (min_s == 0)
			  		as = 0;
	    		else if (s - sat >= min_s_in)
			  		as = 0;
	    		else if ((out_slope != 0) && (s - sat > min_s))
			  		as = (s - sat - min_s) / (min_s * 2);
	    		else if (s - sat > min_s_out)
			  		as = (s - sat - min_s_out) / (min_s_out * 2);
	    		else
			  		has_match = false;
	    	}

// Check if the value is more than the minimun
			if (has_match)
	    	{
	    		if (min_v == 0)
			  		av = 0;
	    		else if (v >= min_v_in)
			  		av = 0;
	    		else if ((out_slope != 0) && (v > min_v))
			  		av = (v - min_v) / (min_v * 2);
	    		else if (v > min_v_out)
			  		av = (v - min_v_out) / (min_v_out * 2);
	    		else
			  		has_match = false;
	    	}

// Check if the value is less than the maximum
		  	if (has_match)
	    	{
	    	  	if (max_v == 0)
					avm = 1;
	    	  	else if (v <= max_v_in)
					avm = 0;
	    	  	else if ((out_slope != 0) && (v < max_v))
					avm = (v - max_v) / (max_v * 2);
	    	  	else if (v < max_v_out)
					avm = (v - max_v_out) / (max_v_out * 2);
	    	  	else
					has_match = false;
	    	}

	  // If the color is part of the key, update the alpha channel
	  if (has_match)
	    a = MAX (MAX (ah, av), MAX (as, avm));

	  // Spill light processing
	  if ((ABS (h - hue_key) < spill_threshold * 180) ||
	      ((ABS (h - hue_key) > 360)
	       && (ABS (h - hue_key) - 360 < spill_threshold * 180)))
	    {
	      s = s * spill_amount * ABS (h - hue_key) / (spill_threshold * 180);

	      HSV::hsv_to_rgb (r, g, b, h, s, v);

		if (!use_yuv) {
			row[0] = (component_type) ((float) r * max);
			row[1] = (component_type) ((float) g * max);
			row[2] = (component_type) ((float) b * max);
		}
		else
			YUV::yuv.rgb_to_yuv_f(r, g, b, row[0], row[1], row[2]);
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
	      row[1] =
		(component_type) ((float) a * (row[1] - (max / 2 + 1)) +
				  max / 2 + 1);
	      row[2] =
		(component_type) ((float) a * (row[2] - (max / 2 + 1)) +
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





REGISTER_PLUGIN(ChromaKeyHSV)



ChromaKeyHSV::ChromaKeyHSV(PluginServer *server)
 : PluginVClient(server)
{

	engine = 0;
}

ChromaKeyHSV::~ChromaKeyHSV()
{

	if(engine) delete engine;
}


int ChromaKeyHSV::process_buffer(VFrame *frame,
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

const char* ChromaKeyHSV::plugin_title() { return N_("Chroma key (HSV)"); }
int ChromaKeyHSV::is_realtime() { return 1; }


LOAD_CONFIGURATION_MACRO(ChromaKeyHSV, ChromaKeyConfig)


void ChromaKeyHSV::save_data(KeyFrame * keyframe)
{
	FileXML output;
	output.set_shared_output(keyframe->xbuf);
	output.tag.set_title("CHROMAKEY_HSV");
	output.tag.set_property("RED", config.red);
	output.tag.set_property("GREEN", config.green);
	output.tag.set_property("BLUE", config.blue);
	output.tag.set_property("MIN_BRIGHTNESS", config.min_brightness);
	output.tag.set_property("MAX_BRIGHTNESS", config.max_brightness);
	output.tag.set_property("SATURATION", config.saturation);
	output.tag.set_property("MIN_SATURATION", config.min_saturation);
	output.tag.set_property("TOLERANCE", config.tolerance);
	output.tag.set_property("IN_SLOPE", config.in_slope);
	output.tag.set_property("OUT_SLOPE", config.out_slope);
	output.tag.set_property("ALPHA_OFFSET", config.alpha_offset);
	output.tag.set_property("SPILL_THRESHOLD", config.spill_threshold);
	output.tag.set_property("SPILL_AMOUNT", config.spill_amount);
	output.tag.set_property("SHOW_MASK", config.show_mask);
	output.append_tag();
	output.tag.set_title("/CHROMAKEY_HSV");
	output.append_tag();
	output.append_newline();
	output.terminate_string();
}

void ChromaKeyHSV::read_data(KeyFrame * keyframe)
{
	FileXML input;

	input.set_shared_input(keyframe->xbuf);

	while( !input.read_tag() ) {
		if( input.tag.title_is("CHROMAKEY_HSV") ) {
			config.red = input.tag.get_property("RED", config.red);
			config.green = input.tag.get_property("GREEN", config.green);
			config.blue = input.tag.get_property("BLUE", config.blue);
			config.min_brightness =
				input.tag.get_property("MIN_BRIGHTNESS", config.min_brightness);
			config.max_brightness =
				input.tag.get_property("MAX_BRIGHTNESS", config.max_brightness);
			config.saturation =
				input.tag.get_property("SATURATION", config.saturation);
			config.min_saturation =
				input.tag.get_property("MIN_SATURATION", config.min_saturation);
			config.tolerance =
				input.tag.get_property("TOLERANCE", config.tolerance);
			config.in_slope =
				input.tag.get_property("IN_SLOPE", config.in_slope);
			config.out_slope =
				input.tag.get_property("OUT_SLOPE", config.out_slope);
			config.alpha_offset =
				input.tag.get_property("ALPHA_OFFSET", config.alpha_offset);
			config.spill_threshold =
				input.tag.get_property("SPILL_THRESHOLD",
							config.spill_threshold);
			config.spill_amount =
				input.tag.get_property("SPILL_AMOUNT", config.spill_amount);
			config.show_mask =
				input.tag.get_property("SHOW_MASK", config.show_mask);
		}
	}
}


NEW_WINDOW_MACRO(ChromaKeyHSV, ChromaKeyWindow)

void ChromaKeyHSV::update_gui()
{
	if( thread ) {
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
	switch(clear) {
		case RESET_RGB :
			update_sample();
			break;
		case RESET_MIN_BRIGHTNESS :
			min_brightness_text->update(config.min_brightness);
			min_brightness_slider->update(config.min_brightness);
			break;
		case RESET_MAX_BRIGHTNESS :
			max_brightness_text->update(config.max_brightness);
			max_brightness_slider->update(config.max_brightness);
			break;
		case RESET_TOLERANCE :
			tolerance_text->update(config.tolerance);
			tolerance_slider->update(config.tolerance);
			break;
		case RESET_SATURATION :
			saturation_text->update(config.saturation);
			saturation_slider->update(config.saturation);
			break;
		case RESET_MIN_SATURATION :
			min_saturation_text->update(config.min_saturation);
			min_saturation_slider->update(config.min_saturation);
			break;
		case RESET_IN_SLOPE :
			in_slope_text->update(config.in_slope);
			in_slope_slider->update(config.in_slope);
			break;
		case RESET_OUT_SLOPE :
			out_slope_text->update(config.out_slope);
			out_slope_slider->update(config.out_slope);
			break;
		case RESET_ALPHA_OFFSET :
			alpha_offset_text->update(config.alpha_offset);
			alpha_offset_slider->update(config.alpha_offset);
			break;
		case RESET_SPILL_THRESHOLD :
			spill_threshold_text->update(config.spill_threshold);
			spill_threshold_slider->update(config.spill_threshold);
			break;
		case RESET_SPILL_AMOUNT :
			spill_amount_text->update(config.spill_amount);
			spill_amount_slider->update(config.spill_amount);
			break;
		case RESET_ALL :
		case RESET_DEFAULT_SETTINGS :
		default:
			min_brightness_text->update(config.min_brightness);
			min_brightness_slider->update(config.min_brightness);
			max_brightness_text->update(config.max_brightness);
			max_brightness_slider->update(config.max_brightness);
			tolerance_text->update(config.tolerance);
			tolerance_slider->update(config.tolerance);
			saturation_text->update(config.saturation);
			saturation_slider->update(config.saturation);
			min_saturation_text->update(config.min_saturation);
			min_saturation_slider->update(config.min_saturation);
			in_slope_text->update(config.in_slope);
			in_slope_slider->update(config.in_slope);
			out_slope_text->update(config.out_slope);
			out_slope_slider->update(config.out_slope);
			alpha_offset_text->update(config.alpha_offset);
			alpha_offset_slider->update(config.alpha_offset);
			spill_threshold_text->update(config.spill_threshold);
			spill_threshold_slider->update(config.spill_threshold);
			spill_amount_text->update(config.spill_amount);
			spill_amount_slider->update(config.spill_amount);
			show_mask->update(config.show_mask);
			update_sample();
			break;
	}
}



int ChromaKeyHSV::handle_opengl()
{
#ifdef HAVE_GL
// For macro
	ChromaKeyHSV *plugin = this;
	OUTER_VARIABLES

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
	extern unsigned char _binary_chromakey_sl_start[];
	static const char *shader_frag = (char*)_binary_chromakey_sl_start;
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
		glUniform1f(glGetUniformLocation(shader, "sat"), sat);
		glUniform1f(glGetUniformLocation(shader, "min_s"), min_s);
		glUniform1f(glGetUniformLocation(shader, "min_s_in"), min_s_in);
		glUniform1f(glGetUniformLocation(shader, "min_s_out"), min_s_out);
		glUniform1f(glGetUniformLocation(shader, "min_v"), min_v);
		glUniform1f(glGetUniformLocation(shader, "min_v_in"), min_v_in);
		glUniform1f(glGetUniformLocation(shader, "min_v_out"), min_v_out);
		glUniform1f(glGetUniformLocation(shader, "max_v"), max_v);
		glUniform1f(glGetUniformLocation(shader, "max_v_in"), max_v_in);
		glUniform1f(glGetUniformLocation(shader, "max_v_out"), max_v_out);
		glUniform1f(glGetUniformLocation(shader, "spill_threshold"), spill_threshold);
		glUniform1f(glGetUniformLocation(shader, "spill_amount"), spill_amount);
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

