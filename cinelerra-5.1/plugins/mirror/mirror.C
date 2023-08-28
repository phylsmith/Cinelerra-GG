
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

/*
 * 2023. Derivative by Flip plugin.
*/

#include "clip.h"
#include "bccmodels.h"
#include "bchash.h"
#include "filexml.h"
#include "mirror.h"
#include "mirrorwindow.h"
#include "language.h"
#include "mwindow.h"

#include <stdint.h>
#include <string.h>
#include <math.h>

REGISTER_PLUGIN(MirrorMain)






MirrorConfig::MirrorConfig()
{
	reset(RESET_DEFAULT_SETTINGS);
}

void MirrorConfig::reset(int clear)
{
	switch(clear) {
		case RESET_REFLECTION_CENTER_X :
			reflection_center_x = 50.00;
			mirror_horizontal = 1;
		        mirror_swaphorizontal = 0;
			break;
		case RESET_REFLECTION_CENTER_Y :
			reflection_center_y = 50.00;
			mirror_vertical = 1;
		        mirror_swapvertical = 0;
			break;
		case RESET_ALL :
		case RESET_DEFAULT_SETTINGS :
		default:
			mirror_horizontal = 1;
		        mirror_swaphorizontal = 0;
			mirror_vertical = 0;
		        mirror_swapvertical = 0;
			reflection_center_enable = 0;
			reflection_center_x = 50.00;
			reflection_center_y = 50.00;
			break;
	}
}

void MirrorConfig::copy_from(MirrorConfig &that)
{
	mirror_horizontal = that.mirror_horizontal;
	mirror_swaphorizontal = that.mirror_swaphorizontal;
	mirror_vertical = that.mirror_vertical;
	mirror_swapvertical = that.mirror_swapvertical;
	reflection_center_enable = that.reflection_center_enable;
	reflection_center_x = that.reflection_center_x;
	reflection_center_y = that.reflection_center_y;
}

int MirrorConfig::equivalent(MirrorConfig &that)
{
	return EQUIV(mirror_horizontal, that.mirror_horizontal) &&
                EQUIV(mirror_swaphorizontal, that.mirror_swaphorizontal) &&
		EQUIV(mirror_vertical, that.mirror_vertical) && 
                EQUIV(mirror_swapvertical, that.mirror_swapvertical) &&
                EQUIV(reflection_center_enable, that.reflection_center_enable) &&
		EQUIV(reflection_center_x, that.reflection_center_x) &&
		EQUIV(reflection_center_y, that.reflection_center_y);
}

void MirrorConfig::interpolate(MirrorConfig &prev,
	MirrorConfig &next,
	int64_t prev_frame,
	int64_t next_frame,
	int64_t current_frame)
{
	double next_scale = (double)(current_frame - prev_frame) / (next_frame - prev_frame);
	double prev_scale = (double)(next_frame - current_frame) / (next_frame - prev_frame);

	this->mirror_horizontal = prev.mirror_horizontal;
        this->mirror_swaphorizontal = prev.mirror_swaphorizontal;
	this->mirror_vertical = prev.mirror_vertical;
        this->mirror_swapvertical = prev.mirror_swapvertical;
	this->reflection_center_enable = prev.reflection_center_enable;
	this->reflection_center_x = prev.reflection_center_x * prev_scale + next.reflection_center_x * next_scale;
	this->reflection_center_y = prev.reflection_center_y * prev_scale + next.reflection_center_y * next_scale;
}










MirrorMain::MirrorMain(PluginServer *server)
 : PluginVClient(server)
{

}

MirrorMain::~MirrorMain()
{

}

const char* MirrorMain::plugin_title() { return N_("Mirror"); }
int MirrorMain::is_realtime() { return 1; }


#define SWAPCOPY_PIXELS(components, in, out) \
{ \
	in[0] = out[0]; \
	in[1] = out[1]; \
	in[2] = out[2]; \
	if(components == 4) \
	{ \
		in[3] = out[3]; \
	} \
}


#define MIRROR_MACRO(type, components) \
{ \
	type **input_rows, **output_rows; \
	type *input_row, *output_row; \
	input_rows = ((type**)frame->get_rows()); \
	output_rows = ((type**)frame->get_rows()); \
 \
	if (!config.reflection_center_enable) \
	{ \
		/* HORIZONTAL ONLY */ \
		if(config.mirror_horizontal && !config.mirror_vertical && !config.mirror_swaphorizontal) \
		{ \
			for(i = 0; i < h; i++) \
			{ \
				input_row = input_rows[i]; \
				output_row = output_rows[i] + (w - 1) * components; \
				for(k = 0; k < w / 2; k++) \
				{ \
					SWAPCOPY_PIXELS(components, output_row, input_row); \
					input_row += components; \
					output_row -= components; \
				} \
			} \
		} \
		/* HORIZONTAL SWAP ONLY */ \
		else if(config.mirror_horizontal && !config.mirror_vertical && config.mirror_swaphorizontal) \
		{ \
			for(i = 0; i < h; i++) \
			{ \
				input_row = input_rows[i]; \
				output_row = output_rows[i] + (w - 1) * components; \
				for(k = 0; k < w / 2; k++) \
				{ \
					SWAPCOPY_PIXELS(components, input_row, output_row); \
					input_row += components; \
					output_row -= components; \
				} \
			} \
		} \
		/* VERTICAL ONLY */ \
		else if(config.mirror_vertical && !config.mirror_horizontal && !config.mirror_swapvertical) \
		{ \
			for(i = 0, j = h - 1; i < h / 2; i++, j--) \
			{ \
				input_row = input_rows[i]; \
				output_row = output_rows[j]; \
				for(k = 0; k < w; k++) \
				{ \
					SWAPCOPY_PIXELS(components, output_row, input_row); \
					output_row += components; \
					input_row += components; \
				} \
			} \
		} \
		/* VERTICAL SWAP ONLY */ \
		else if(config.mirror_vertical && !config.mirror_horizontal && config.mirror_swapvertical) \
		{ \
			for(i = 0, j = h - 1; i < h / 2; i++, j--) \
			{ \
				input_row = input_rows[i]; \
				output_row = output_rows[j]; \
				for(k = 0; k < w; k++) \
				{ \
					SWAPCOPY_PIXELS(components, input_row, output_row); \
					output_row += components; \
					input_row += components; \
				} \
			} \
		} \
		/* HORIZONTAL and VERTICAL */ \
		else if(config.mirror_horizontal && config.mirror_vertical && !config.mirror_swaphorizontal && !config.mirror_swapvertical) \
		{ \
			/* HORIZONTAL ONLY */ \
			for(i = 0; i < h; i++) \
			{ \
				input_row = input_rows[i]; \
				output_row = output_rows[i] + (w - 1) * components; \
				for(k = 0; k < w / 2; k++) \
				{ \
					SWAPCOPY_PIXELS(components, output_row, input_row); \
					input_row += components; \
					output_row -= components; \
				} \
			} \
			/* VERTICAL ONLY */ \
			for(i = 0, j = h - 1; i < h / 2; i++, j--) \
			{ \
				input_row = input_rows[i]; \
				output_row = output_rows[j]; \
				for(k = 0; k < w; k++) \
				{ \
					SWAPCOPY_PIXELS(components, output_row, input_row); \
					output_row += components; \
					input_row += components; \
				} \
			} \
		} \
		/* HORIZONTAL with SWAP and VERTICAL with SWAP */ \
		else if(config.mirror_horizontal && config.mirror_vertical && config.mirror_swaphorizontal && config.mirror_swapvertical) \
		{ \
			/* HORIZONTAL SWAP ONLY */ \
			for(i = 0; i < h; i++) \
			{ \
				input_row = input_rows[i]; \
				output_row = output_rows[i] + (w - 1) * components; \
				for(k = 0; k < w / 2; k++) \
				{ \
					SWAPCOPY_PIXELS(components, input_row, output_row); \
					input_row += components; \
					output_row -= components; \
				} \
			} \
			/* VERTICAL SWAP ONLY */ \
			for(i = 0, j = h - 1; i < h / 2; i++, j--) \
			{ \
				input_row = input_rows[i]; \
				output_row = output_rows[j]; \
				for(k = 0; k < w; k++) \
				{ \
					SWAPCOPY_PIXELS(components, input_row, output_row); \
					output_row += components; \
					input_row += components; \
				} \
			} \
		} \
		/* HORIZONTAL with SWAP and VERTICAL ONLY */ \
		else if(config.mirror_horizontal && config.mirror_vertical && config.mirror_swaphorizontal && !config.mirror_swapvertical) \
		{ \
			/* HORIZONTAL SWAP ONLY */ \
			for(i = 0; i < h; i++) \
			{ \
				input_row = input_rows[i]; \
				output_row = output_rows[i] + (w - 1) * components; \
				for(k = 0; k < w / 2; k++) \
				{ \
					SWAPCOPY_PIXELS(components, input_row, output_row); \
					input_row += components; \
					output_row -= components; \
				} \
			} \
			/* VERTICAL ONLY */ \
			for(i = 0, j = h - 1; i < h / 2; i++, j--) \
			{ \
				input_row = input_rows[i]; \
				output_row = output_rows[j]; \
				for(k = 0; k < w; k++) \
				{ \
					SWAPCOPY_PIXELS(components, output_row, input_row); \
					output_row += components; \
					input_row += components; \
				} \
			} \
		} \
		/* HORIZONTAL ONLY and VERTICAL with SWAP */ \
		else if(config.mirror_horizontal && config.mirror_vertical && !config.mirror_swaphorizontal && config.mirror_swapvertical) \
		{ \
			/* HORIZONTAL ONLY */ \
			for(i = 0; i < h; i++) \
			{ \
				input_row = input_rows[i]; \
				output_row = output_rows[i] + (w - 1) * components; \
				for(k = 0; k < w / 2; k++) \
				{ \
					SWAPCOPY_PIXELS(components, output_row, input_row); \
					input_row += components; \
					output_row -= components; \
				} \
			} \
			/* VERTICAL SWAP ONLY */ \
			for(i = 0, j = h - 1; i < h / 2; i++, j--) \
			{ \
				input_row = input_rows[i]; \
				output_row = output_rows[j]; \
				for(k = 0; k < w; k++) \
				{ \
					SWAPCOPY_PIXELS(components, input_row, output_row); \
					output_row += components; \
					input_row += components; \
				} \
			} \
		} \
	} \
 \
	else if(config.reflection_center_enable) \
	{ \
		if(config.mirror_vertical) \
		{ \
			int y_pixel = (int)round(config.reflection_center_y / 100.00 * h); \
			if(config.reflection_center_y < 50.00) \
			{ \
				int valueY_source = y_pixel + 1; \
				int valueY_destination = y_pixel - 1; \
				for(int i = 0; i < y_pixel; i++) \
				{ \
					input_row = input_rows[valueY_source]; \
					output_row = output_rows[valueY_destination]; \
					for(k = 0; k < w; k++) \
					{ \
						SWAPCOPY_PIXELS(components, output_row, input_row); \
						output_row += components; \
						input_row += components; \
					} \
					valueY_source += 1; \
					valueY_destination -= 1; \
				} \
			} \
			else if(config.reflection_center_y > 50.00) \
			{ \
				int valueY_source = y_pixel - 1; \
				int valueY_destination = y_pixel + 1; \
				for(int i = 0; i < (h - y_pixel - 1); i++) \
				{ \
					input_row = input_rows[valueY_source]; \
					output_row = output_rows[valueY_destination]; \
					for(k = 0; k < w; k++) \
					{ \
						SWAPCOPY_PIXELS(components, output_row, input_row); \
						output_row += components; \
						input_row += components; \
					} \
					valueY_source -= 1; \
					valueY_destination += 1; \
				} \
			} \
			else /* Y_PERCENT == 50.00 */ \
			{ \
				for(i = 0, j = h - 1; i < h / 2; i++, j--) \
				{ \
					input_row = input_rows[i]; \
					output_row = output_rows[j]; \
					for(k = 0; k < w; k++) \
					{ \
						SWAPCOPY_PIXELS(components, output_row, input_row); \
						output_row += components; \
						input_row += components; \
					} \
				} \
			} \
		} \
 \
		if(config.mirror_horizontal) \
		{ \
			int x_pixel = (int)round(config.reflection_center_x / 100.00 * w); \
			if(config.reflection_center_x < 50.00) \
			{ \
				int valueX_source = x_pixel + 1; \
				int valueX_destination = x_pixel - 1; \
				for(i = 0; i < h; i++) \
				{ \
					input_row = input_rows[i] + valueX_source * components; \
					output_row = output_rows[i] + valueX_destination * components; \
					for(k = 0; k < x_pixel; k++) \
					{ \
						SWAPCOPY_PIXELS(components, output_row, input_row); \
						output_row -= components; \
						input_row += components; \
					} \
				} \
			} \
			else if(config.reflection_center_x > 50.00) \
			{ \
				int valueX_source = x_pixel - 1; \
				int valueX_destination = x_pixel + 1; \
				for(i = 0; i < h; i++) \
				{ \
					input_row = input_rows[i] + valueX_source * components; \
					output_row = output_rows[i] + valueX_destination * components; \
					for(k = 0; k < (w - x_pixel - 1); k++) \
					{ \
						SWAPCOPY_PIXELS(components, output_row, input_row); \
						output_row += components; \
						input_row -= components; \
					} \
				} \
			} \
			else /* X_PERCENT == 50.00 */ \
			{ \
				for(i = 0; i < h; i++) \
				{ \
					input_row = input_rows[i]; \
					output_row = output_rows[i] + (w - 1) * components; \
					for(k = 0; k < w / 2; k++) \
					{ \
						SWAPCOPY_PIXELS(components, output_row, input_row); \
						input_row += components; \
						output_row -= components; \
					} \
				} \
			} \
		} \
	} \
}

int MirrorMain::process_buffer(VFrame *frame,
		int64_t start_position,
		double frame_rate)
{
	int i, j, k;
	int w = frame->get_w();
	int h = frame->get_h();
	int colormodel = frame->get_color_model();

	load_configuration();

// THIS CODE WAS FOR the FLIP Plugin!!!
/*
	read_frame(frame,
		0,
		get_source_position(),
		get_framerate(),
		get_use_opengl());

	if(get_use_opengl())
	{
		if(config.mirror_vertical || config.mirror_horizontal)
			return run_opengl();
		else
			return 0;
	}
*/
	read_frame(frame,
		0,
		start_position,
		frame_rate,
		get_use_opengl());

	if(config.mirror_vertical || config.mirror_horizontal)
		{
		switch(colormodel)
		{
			case BC_RGB888:
			case BC_YUV888:
				MIRROR_MACRO(unsigned char, 3);
				break;
			case BC_RGB_FLOAT:
				MIRROR_MACRO(float, 3);
				break;
			case BC_RGB161616:
			case BC_YUV161616:
				MIRROR_MACRO(uint16_t, 3);
				break;
			case BC_RGBA8888:
			case BC_YUVA8888:
				MIRROR_MACRO(unsigned char, 4);
				break;
			case BC_RGBA_FLOAT:
				MIRROR_MACRO(float, 4);
				break;
			case BC_RGBA16161616:
			case BC_YUVA16161616:
				MIRROR_MACRO(uint16_t, 4);
				break;
		}
	}
	return 0;
}


NEW_WINDOW_MACRO(MirrorMain, MirrorWindow)
LOAD_CONFIGURATION_MACRO(MirrorMain, MirrorConfig)

void MirrorMain::update_gui()
{
	if(thread)
	{
		load_configuration();
		thread->window->lock_window();
		((MirrorWindow*)thread->window)->mirror_horizontal->update((int)config.mirror_horizontal);
		((MirrorWindow*)thread->window)->mirror_swaphorizontal->update((int)config.mirror_swaphorizontal);
		((MirrorWindow*)thread->window)->mirror_vertical->update((int)config.mirror_vertical);
		((MirrorWindow*)thread->window)->mirror_swapvertical->update((int)config.mirror_swapvertical);
		((MirrorWindow*)thread->window)->reflection_center_enable->update(config.reflection_center_enable);
		((MirrorWindow*)thread->window)->reflection_center_x_text->update(config.reflection_center_x);
		((MirrorWindow*)thread->window)->reflection_center_x_slider->update(config.reflection_center_x);
		((MirrorWindow*)thread->window)->reflection_center_y_text->update(config.reflection_center_y);
		((MirrorWindow*)thread->window)->reflection_center_y_slider->update(config.reflection_center_y);
		// Needed to update Enable-Disable GUI when "Show controls" is pressed.
		((MirrorWindow*)thread->window)->update_reflection_center_enable();
		thread->window->unlock_window();
	}
}

void MirrorMain::save_data(KeyFrame *keyframe)
{
	FileXML output;

// cause data to be stored directly in text
	output.set_shared_output(keyframe->xbuf);

// Store data
	output.tag.set_title("MIRROR");
	output.tag.set_property("HORIZONTAL", config.mirror_horizontal);
	output.tag.set_property("SWAP_HORIZONTAL", config.mirror_swaphorizontal);
	output.tag.set_property("VERTICAL", config.mirror_vertical);
	output.tag.set_property("SWAP_VERTICAL", config.mirror_swapvertical);
	output.tag.set_property("REFLECTION_CENTER_ENABLE", config.reflection_center_enable);
	output.tag.set_property("REFLECTION_CENTER_X", config.reflection_center_x);
	output.tag.set_property("REFLECTION_CENTER_Y", config.reflection_center_y);
	output.append_tag();
	output.tag.set_title("/MIRROR");
	output.append_tag();
	output.append_newline();
	output.terminate_string();
// data is now in *text
}

void MirrorMain::read_data(KeyFrame *keyframe)
{
	FileXML input;

	input.set_shared_input(keyframe->xbuf);

	int result = 0;
	config.mirror_vertical = config.mirror_horizontal = 0;
	config.mirror_swapvertical = config.mirror_swaphorizontal = 0;

	while(!result)
	{
		result = input.read_tag();

		if(!result)
		{
			if(input.tag.title_is("MIRROR"))
			{
				config.mirror_horizontal = input.tag.get_property("HORIZONTAL", config.mirror_horizontal);
				config.mirror_swaphorizontal = input.tag.get_property("SWAP_HORIZONTAL", config.mirror_swaphorizontal);
				config.mirror_vertical = input.tag.get_property("VERTICAL", config.mirror_vertical);
				config.mirror_swapvertical = input.tag.get_property("SWAP_VERTICAL", config.mirror_swapvertical);
				config.reflection_center_enable = input.tag.get_property("REFLECTION_CENTER_ENABLE", config.reflection_center_enable);
				config.reflection_center_x = input.tag.get_property("REFLECTION_CENTER_X", config.reflection_center_x);
				config.reflection_center_y = input.tag.get_property("REFLECTION_CENTER_Y", config.reflection_center_y);
			}
		}
	}
}


int MirrorMain::handle_opengl()
{
#ifdef HAVE_GL
// FIXIT. When?! ... THIS CODE WAS FOR the FLIP Plugin!!!
/*
	get_output()->to_texture();
	get_output()->enable_opengl();
	get_output()->init_screen();
	get_output()->bind_texture(0);

	if(config.mirror_vertical && !config.mirror_horizontal)
	{
		get_output()->draw_texture(0,
			0,
			get_output()->get_w(),
			get_output()->get_h(),
			0,
			get_output()->get_h(),
			get_output()->get_w(),
			0);
	}

	if(config.mirror_horizontal && !config.mirror_vertical)
	{
		get_output()->draw_texture(0,
			0,
			get_output()->get_w(),
			get_output()->get_h(),
			get_output()->get_w(),
			0,
			0,
			get_output()->get_h());
	}

	if(config.mirror_vertical && config.mirror_horizontal)
	{
		get_output()->draw_texture(0,
			0,
			get_output()->get_w(),
			get_output()->get_h(),
			get_output()->get_w(),
			get_output()->get_h(),
			0,
			0);
	}

	get_output()->set_opengl_state(VFrame::SCREEN);
*/
#endif
	return 0;
}



