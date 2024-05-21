
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

#include "bccolors.h"
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



ChromaKeyConfig::ChromaKeyConfig()
{
       reset(RESET_DEFAULT_SETTINGS);
}

void ChromaKeyConfig::reset(int clear)

{
	switch(clear) {
		case RESET_ALL :
			red = 0.0;
			green = 0.0;
			blue = 0.0;
			threshold = 0.0;
			use_value = 0;
			slope = 0.0;
			break;
		case RESET_RGB :
			red = 0.0;
			green = 0.0;
			blue = 0.0;
			break;
		case RESET_SLOPE :
			slope = 0.0;
			break;
		case RESET_THRESHOLD :
			threshold = 0.0;
			break;
		case RESET_DEFAULT_SETTINGS :
		default:
			red = 0.0;
			green = 0.0;
			blue = 0.0;
			threshold = 10.0;
			use_value = 0;
			slope = 0.0;
			break;
	}
}


void ChromaKeyConfig::copy_from(ChromaKeyConfig &src)
{
	red = src.red;
	green = src.green;
	blue = src.blue;
	threshold = src.threshold;
	use_value = src.use_value;
	slope = src.slope;
}

int ChromaKeyConfig::equivalent(ChromaKeyConfig &src)
{
	return (EQUIV(red, src.red) &&
		EQUIV(green, src.green) &&
		EQUIV(blue, src.blue) &&
		EQUIV(threshold, src.threshold) &&
		EQUIV(slope, src.slope) &&
		use_value == src.use_value);
}

void ChromaKeyConfig::interpolate(ChromaKeyConfig &prev,
	ChromaKeyConfig &next,
	int64_t prev_frame,
	int64_t next_frame,
	int64_t current_frame)
{
	double next_scale = (double)(current_frame - prev_frame) / (next_frame - prev_frame);
	double prev_scale = (double)(next_frame - current_frame) / (next_frame - prev_frame);

	this->red = prev.red * prev_scale + next.red * next_scale;
	this->green = prev.green * prev_scale + next.green * next_scale;
	this->blue = prev.blue * prev_scale + next.blue * next_scale;
	this->threshold = prev.threshold * prev_scale + next.threshold * next_scale;
	this->slope = prev.slope * prev_scale + next.slope * next_scale;
	this->use_value = prev.use_value;
}

int ChromaKeyConfig::get_color()
{
	int red = (int)(CLIP(this->red, 0, 1) * 0xff);
	int green = (int)(CLIP(this->green, 0, 1) * 0xff);
	int blue = (int)(CLIP(this->blue, 0, 1) * 0xff);
	return (red << 16) | (green << 8) | blue;
}







ChromaKeyWindow::ChromaKeyWindow(ChromaKey *plugin)
 : PluginClientWindow(plugin,
	xS(420),
	yS(250),
	xS(420),
	yS(250),
	0)
{
	this->plugin = plugin;
	color_thread = 0;
}

ChromaKeyWindow::~ChromaKeyWindow()
{
	delete color_thread;
}

void ChromaKeyWindow::create_objects()
{
	int xs10 = xS(10), xs20 = xS(20), xs100 = xS(100), xs200 = xS(200);
	int ys10 = yS(10), ys20 = yS(20), ys30 = yS(30), ys40 = yS(40), ys50 = yS(50);
	int x = xs10, y = ys10, x2 = xS(80), x3 = xS(180);
	int clr_x = get_w()-x - xS(22); // note: clrBtn_w = 22
	int defaultBtn_w = xs100;

	BC_Title *title;
	BC_TitleBar *title_bar;
	BC_Bar *bar;

// Color section
	add_subwindow(title_bar = new BC_TitleBar(x, y, get_w()-2*x, xs20, xs10, _("Color")));
	y += ys20;
	add_subwindow(color = new ChromaKeyColor(plugin, this, x, y));
	// Info for Sample rectangle:       x_slider w_slider w_sample
	//                                        \       |      /    y,   w,     h
	add_subwindow(sample = new BC_SubWindow(x3 + xs200 - xs100, y, xs100, ys50));
	y += ys30;
	add_subwindow(use_colorpicker = new ChromaKeyUseColorPicker(plugin, this, x, y));

// Key parameters section
	y += ys30;
	add_subwindow(title_bar = new BC_TitleBar(x, y, get_w()-2*x, xs20, xs10, _("Key parameters")));
	y += ys20;
	add_subwindow(title = new BC_Title(x, y, _("Threshold:")));
	threshold_text = new ChromaKeyFText(plugin, this,
		0, &(plugin->config.threshold), (x + x2), y, MIN_VALUE, MAX_VALUE);
	threshold_text->create_objects();
	threshold_slider = new ChromaKeyFSlider(plugin,
		threshold_text, &(plugin->config.threshold), x3, y, MIN_VALUE, MAX_VALUE, xs200);
	add_subwindow(threshold_slider);
	threshold_text->slider = threshold_slider;
	add_subwindow(threshold_Clr = new ChromaKeyClr(plugin, this, clr_x, y, RESET_THRESHOLD));
	y += ys30;

	add_subwindow(title = new BC_Title(x, y, _("Slope:")));
	slope_text = new ChromaKeyFText(plugin, this,
		0, &(plugin->config.slope), (x + x2), y, MIN_VALUE, MAX_VALUE);
	slope_text->create_objects();
	slope_slider = new ChromaKeyFSlider(plugin,
		slope_text, &(plugin->config.slope), x3, y, MIN_VALUE, MAX_VALUE, xs200);
	add_subwindow(slope_slider);
	slope_text->slider = slope_slider;
	add_subwindow(slope_Clr = new ChromaKeyClr(plugin, this, clr_x, y, RESET_SLOPE));
	y += ys30;

	add_subwindow(use_value = new ChromaKeyUseValue(plugin, x, y));
	y += ys40;

// Reset section
	add_subwindow(bar = new BC_Bar(x, y, get_w()-2*x));
	y += ys10;
	add_subwindow(reset = new ChromaKeyReset(plugin, this, x, y));
	add_subwindow(default_settings = new ChromaKeyDefaultSettings(plugin, this,
		(get_w() - xs10 - defaultBtn_w), y, defaultBtn_w));

	color_thread = new ChromaKeyColorThread(plugin, this);

	update_sample();
	show_window();
	flush();
}


void ChromaKeyWindow::update_sample()
{
	sample->set_color(plugin->config.get_color());
	sample->draw_box(0,
		0,
		sample->get_w(),
		sample->get_h());
	sample->set_color(BLACK);
	sample->draw_rectangle(0,
		0,
		sample->get_w(),
		sample->get_h());
	sample->flash();
}

void ChromaKeyWindow::done_event(int result)
{
	color_thread->close_window();
}









ChromaKeyColor::ChromaKeyColor(ChromaKey *plugin,
	ChromaKeyWindow *gui,
	int x,
	int y)
 : BC_GenericButton(x,
	y,
	_("Color..."))
{
	this->plugin = plugin;
	this->gui = gui;
}
int ChromaKeyColor::handle_event()
{
	gui->color_thread->start_window(
		plugin->config.get_color(),
		0xff);
	return 1;
}



ChromaKeyFText::ChromaKeyFText(ChromaKey *plugin, ChromaKeyWindow *gui,
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

ChromaKeyFSlider::ChromaKeyFSlider(ChromaKey *plugin,
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

ChromaKeyClr::ChromaKeyClr(ChromaKey *plugin, ChromaKeyWindow *gui, int x, int y, int clear)
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



ChromaKeyUseValue::ChromaKeyUseValue(ChromaKey *plugin, int x, int y)
 : BC_CheckBox(x, y, plugin->config.use_value, _("Use value"))
{
	this->plugin = plugin;
}
int ChromaKeyUseValue::handle_event()
{
	plugin->config.use_value = get_value();
	plugin->send_configure_change();
	return 1;
}

ChromaKeyReset::ChromaKeyReset(ChromaKey *plugin, ChromaKeyWindow *gui, int x, int y)
 : BC_GenericButton(x, y, _("Reset"))
{
	this->plugin = plugin;
	this->gui = gui;
}

int ChromaKeyReset::handle_event()
{
	plugin->config.reset(RESET_ALL);
	gui->update_gui(RESET_ALL);
	plugin->send_configure_change();
	return 1;
}

ChromaKeyDefaultSettings::ChromaKeyDefaultSettings(ChromaKey *plugin, ChromaKeyWindow *gui,
	int x, int y, int w)
 : BC_GenericButton(x, y, w, _("Default"))
{
	this->plugin = plugin;
	this->gui = gui;
}
ChromaKeyDefaultSettings::~ChromaKeyDefaultSettings()
{
}
int ChromaKeyDefaultSettings::handle_event()
{
	plugin->config.reset(RESET_DEFAULT_SETTINGS);
	gui->update_gui(RESET_DEFAULT_SETTINGS);
	plugin->send_configure_change();
	return 1;
}

ChromaKeyUseColorPicker::ChromaKeyUseColorPicker(ChromaKey *plugin,
	ChromaKeyWindow *gui,
	int x,
	int y)
 : BC_GenericButton(x, y, _("Use color picker"))
{
	this->plugin = plugin;
	this->gui = gui;
}

int ChromaKeyUseColorPicker::handle_event()
{
	plugin->config.red = plugin->get_red();
	plugin->config.green = plugin->get_green();
	plugin->config.blue = plugin->get_blue();
	gui->update_sample();
	plugin->send_configure_change();
	return 1;
}




ChromaKeyColorThread::ChromaKeyColorThread(ChromaKey *plugin, ChromaKeyWindow *gui)
 : ColorPicker(1, _("Inner color"))
{
	this->plugin = plugin;
	this->gui = gui;
}

int ChromaKeyColorThread::handle_new_color(int output, int alpha)
{
	plugin->config.red = (float)(output & 0xff0000) / 0xff0000;
	plugin->config.green = (float)(output & 0xff00) / 0xff00;
	plugin->config.blue = (float)(output & 0xff) / 0xff;
	gui->lock_window("ChromaKeyColorThread::handle_new_color");
	gui->update_sample();
	gui->unlock_window();
	plugin->send_configure_change();
	return 1;
}










ChromaKeyServer::ChromaKeyServer(ChromaKey *plugin)
 : LoadServer(plugin->PluginClient::smp + 1, plugin->PluginClient::smp + 1)
{
	this->plugin = plugin;
}
void ChromaKeyServer::init_packages()
{
	for(int i = 0; i < get_total_packages(); i++)
	{
		ChromaKeyPackage *pkg = (ChromaKeyPackage*)get_package(i);
		pkg->y1 = plugin->input->get_h() * i / get_total_packages();
		pkg->y2 = plugin->input->get_h() * (i + 1) / get_total_packages();
	}

}
LoadClient* ChromaKeyServer::new_client()
{
	return new ChromaKeyUnit(plugin, this);
}
LoadPackage* ChromaKeyServer::new_package()
{
	return new ChromaKeyPackage;
}



ChromaKeyPackage::ChromaKeyPackage()
 : LoadPackage()
{
}

ChromaKeyUnit::ChromaKeyUnit(ChromaKey *plugin, ChromaKeyServer *server)
 : LoadClient(server)
{
	this->plugin = plugin;
}


void ChromaKeyUnit::process_package(LoadPackage *package)
{
	ChromaKeyPackage *pkg = (ChromaKeyPackage*)package;

	int w = plugin->input->get_w();

	float h, s, v;
	HSV::rgb_to_hsv(plugin->config.red,
		plugin->config.green,
		plugin->config.blue,
		h,
		s,
		v);
 	//float min_hue = h - plugin->config.threshold * 360 / 100;
 	//float max_hue = h + plugin->config.threshold * 360 / 100;


#define RGB_TO_VALUE(r, g, b) YUV::yuv.rgb_to_y_f((r),(g),(b))

#define OUTER_VARIABLES(plugin) \
	float value = RGB_TO_VALUE(plugin->config.red, \
		plugin->config.green, \
		plugin->config.blue); \
	float threshold = plugin->config.threshold / 100; \
	float min_v = value - threshold; \
	float max_v = value + threshold; \
	float r_key = plugin->config.red; \
	float g_key = plugin->config.green; \
	float b_key = plugin->config.blue; \
	int y_key, u_key, v_key; \
	YUV::yuv.rgb_to_yuv_8( \
		(int)(r_key * 0xff), (int)(g_key * 0xff), (int)(b_key * 0xff), \
		y_key, u_key, v_key); \
	float run = plugin->config.slope / 100; \
	float threshold_run = threshold + run;

	OUTER_VARIABLES(plugin)



#define CHROMAKEY(type, components, max, use_yuv) \
{ \
	for(int i = pkg->y1; i < pkg->y2; i++) \
	{ \
		type *row = (type*)plugin->input->get_rows()[i]; \
 \
		for(int j = 0; j < w; j++) \
		{ \
			float a = 1; \
 \
/* Test for value in range */ \
			if(plugin->config.use_value) \
			{ \
				float current_value; \
				if(use_yuv) \
				{ \
					float r = (float)row[0] / max; \
					current_value = r; \
				} \
				else \
				{ \
					float r = (float)row[0] / max; \
					float g = (float)row[1] / max; \
					float b = (float)row[2] / max; \
					current_value = RGB_TO_VALUE(r, g, b); \
				} \
 \
/* Full transparency if in range */ \
				if(current_value >= min_v && current_value < max_v) \
				{ \
					a = 0; \
				} \
				else \
/* Phased out if below or above range */ \
				if(current_value < min_v) \
				{ \
					if(min_v - current_value < run) \
						a = (min_v - current_value) / run; \
				} \
				else \
				if(current_value - max_v < run) \
					a = (current_value - max_v) / run; \
			} \
			else \
/* Use color cube */ \
			{ \
				float difference; \
				if(use_yuv) \
				{ \
					type y = row[0]; \
					type u = row[1]; \
					type v = row[2]; \
					difference = sqrt(SQR(y - y_key) + \
						SQR(u - u_key) + \
						SQR(v - v_key)) / max; \
				} \
				else \
				{ \
					float r = (float)row[0] / max; \
					float g = (float)row[1] / max; \
					float b = (float)row[2] / max; \
					difference = sqrt(SQR(r - r_key) +  \
						SQR(g - g_key) + \
						SQR(b - b_key)); \
				} \
				if(difference < threshold) \
				{ \
					a = 0; \
				} \
				else \
				if(difference < threshold_run) \
				{ \
					a = (difference - threshold) / run; \
				} \
 \
			} \
 \
/* Multiply alpha and put back in frame */ \
			if(components == 4) \
			{ \
				row[3] = MIN((type)(a * max), row[3]); \
			} \
			else \
			if(use_yuv) \
			{ \
				row[0] = (type)(a * row[0]); \
				row[1] = (type)(a * (row[1] - (max / 2 + 1)) + max / 2 + 1); \
				row[2] = (type)(a * (row[2] - (max / 2 + 1)) + max / 2 + 1); \
			} \
			else \
			{ \
				row[0] = (type)(a * row[0]); \
				row[1] = (type)(a * row[1]); \
				row[2] = (type)(a * row[2]); \
			} \
 \
			row += components; \
		} \
	} \
}




	switch(plugin->input->get_color_model())
	{
		case BC_RGB_FLOAT:
			CHROMAKEY(float, 3, 1.0, 0);
			break;
		case BC_RGBA_FLOAT:
			CHROMAKEY(float, 4, 1.0, 0);
			break;
		case BC_RGB888:
			CHROMAKEY(unsigned char, 3, 0xff, 0);
			break;
		case BC_RGBA8888:
			CHROMAKEY(unsigned char, 4, 0xff, 0);
			break;
		case BC_YUV888:
			CHROMAKEY(unsigned char, 3, 0xff, 1);
			break;
		case BC_YUVA8888:
			CHROMAKEY(unsigned char, 4, 0xff, 1);
			break;
		case BC_YUV161616:
			CHROMAKEY(uint16_t, 3, 0xffff, 1);
			break;
		case BC_YUVA16161616:
			CHROMAKEY(uint16_t, 4, 0xffff, 1);
			break;
	}

}





REGISTER_PLUGIN(ChromaKey)



ChromaKey::ChromaKey(PluginServer *server)
 : PluginVClient(server)
{

	engine = 0;
}

ChromaKey::~ChromaKey()
{

	delete engine;
}


int ChromaKey::process_buffer(VFrame *frame,
		int64_t start_position,
		double frame_rate)
{
SET_TRACE

	load_configuration();
	this->input = frame;
	this->output = frame;

	read_frame(frame,
		0,
		start_position,
		frame_rate,
		get_use_opengl());

	if(EQUIV(config.threshold, 0))
	{
		return 1;
	}
	else
	{
		if(get_use_opengl()) return run_opengl();

		if(!engine) engine = new ChromaKeyServer(this);
		engine->process_packages();
	}
SET_TRACE

	return 1;
}

const char* ChromaKey::plugin_title() { return N_("Chroma key"); }
int ChromaKey::is_realtime() { return 1; }

NEW_WINDOW_MACRO(ChromaKey, ChromaKeyWindow)

LOAD_CONFIGURATION_MACRO(ChromaKey, ChromaKeyConfig)


void ChromaKey::save_data(KeyFrame *keyframe)
{
	FileXML output;
	output.set_shared_output(keyframe->xbuf);
	output.tag.set_title("CHROMAKEY");
	output.tag.set_property("RED", config.red);
	output.tag.set_property("GREEN", config.green);
	output.tag.set_property("BLUE", config.blue);
	output.tag.set_property("THRESHOLD", config.threshold);
	output.tag.set_property("SLOPE", config.slope);
	output.tag.set_property("USE_VALUE", config.use_value);
	output.append_tag();
	output.tag.set_title("/CHROMAKEY");
	output.append_tag();
	output.append_newline();
	output.terminate_string();
}

void ChromaKey::read_data(KeyFrame *keyframe)
{
	FileXML input;

	input.set_shared_input(keyframe->xbuf);

	while(!input.read_tag())
	{
		if(input.tag.title_is("CHROMAKEY"))
		{
			config.red = input.tag.get_property("RED", config.red);
			config.green = input.tag.get_property("GREEN", config.green);
			config.blue = input.tag.get_property("BLUE", config.blue);
			config.threshold = input.tag.get_property("THRESHOLD", config.threshold);
			config.slope = input.tag.get_property("SLOPE", config.slope);
			config.use_value = input.tag.get_property("USE_VALUE", config.use_value);
		}
	}
}



void ChromaKey::update_gui()
{
	if(thread)
	{
		load_configuration();
		thread->window->lock_window();
		((ChromaKeyWindow *)(thread->window))->update_gui(RESET_ALL);
		thread->window->unlock_window();
	}
}

void ChromaKeyWindow::update_gui(int clear)
{
	ChromaKeyConfig &config = plugin->config;
	switch(clear) {
		case RESET_RGB :
			update_sample();
			break;
		case RESET_SLOPE :
			slope_text->update(config.slope);
			slope_slider->update(config.slope);
			break;
		case RESET_THRESHOLD :
			threshold_text->update(config.threshold);
			threshold_slider->update(config.threshold);
			break;
		case RESET_ALL :
		case RESET_DEFAULT_SETTINGS :
		default:
			update_sample();
			slope_text->update(config.slope);
			slope_slider->update(config.slope);
			threshold_text->update(config.threshold);
			threshold_slider->update(config.threshold);
			use_value->update(config.use_value);
			break;
	}
}

int ChromaKey::handle_opengl()
{
#ifdef HAVE_GL
	OUTER_VARIABLES(this)



	static const char *uniform_frag =
		"uniform sampler2D tex;\n"
		"uniform float min_v;\n"
		"uniform float max_v;\n"
		"uniform float run;\n"
		"uniform float threshold;\n"
		"uniform float threshold_run;\n"
		"uniform vec3 key;\n";

	static const char *get_yuvvalue_frag =
		"float get_value(vec4 color)\n"
		"{\n"
		"	return abs(color.r);\n"
		"}\n";

	static const char *get_rgbvalue_frag =
		"uniform vec3 rgb_to_y_vector;\n"
		"uniform float yminf;\n"
		"float get_value(vec4 color)\n"
		"{\n"
		"	return dot(color.rgb, rgb_to_y_vector) + yminf;\n"
		"}\n";

	static const char *value_frag =
		"void main()\n"
		"{\n"
		"	vec4 color = texture2D(tex, gl_TexCoord[0].st);\n"
		"	float value = get_value(color);\n"
		"	float alpha = 1.0;\n"
		"\n"
		"	if(value >= min_v && value < max_v)\n"
		"		alpha = 0.0;\n"
		"	else\n"
		"	if(value < min_v)\n"
		"	{\n"
		"		if(min_v - value < run)\n"
		"			alpha = (min_v - value) / run;\n"
		"	}\n"
		"	else\n"
		"	if(value - max_v < run)\n"
		"		alpha = (value - max_v) / run;\n"
		"\n"
		"	gl_FragColor = vec4(color.rgb, alpha);\n"
		"}\n";

	static const char *cube_frag =
		"void main()\n"
		"{\n"
		"	vec4 color = texture2D(tex, gl_TexCoord[0].st);\n"
		"	float difference = length(color.rgb - key);\n"
		"	float alpha = 1.0;\n"
		"	if(difference < threshold)\n"
		"		alpha = 0.0;\n"
		"	else\n"
		"	if(difference < threshold_run)\n"
		"		alpha = (difference - threshold) / run;\n"
		"	gl_FragColor = vec4(color.rgb, min(color.a, alpha));\n"
		"}\n";



	get_output()->to_texture();
	get_output()->enable_opengl();
	get_output()->init_screen();

        const char *shader_stack[16];
        memset(shader_stack,0, sizeof(shader_stack));
        int current_shader = 0;
	shader_stack[current_shader++] = uniform_frag;

	switch(get_output()->get_color_model()) {
	case BC_YUV888:
	case BC_YUVA8888:
		if( config.use_value ) {
			shader_stack[current_shader++] = get_yuvvalue_frag;
			shader_stack[current_shader++] = value_frag;
		}
		else {
			shader_stack[current_shader++] = cube_frag;
		}
		break;

	default:
		if(config.use_value) {
			shader_stack[current_shader++] = get_rgbvalue_frag;
			shader_stack[current_shader++] = value_frag;
		}
		else {
			shader_stack[current_shader++] = cube_frag;
		}
		break;
	}
SET_TRACE

	shader_stack[current_shader] = 0;
	unsigned int shader = VFrame::make_shader(shader_stack);
	if( shader > 0 ) {
		get_output()->bind_texture(0);
		glUseProgram(shader);
		glUniform1i(glGetUniformLocation(shader, "tex"), 0);
		glUniform1f(glGetUniformLocation(shader, "min_v"), min_v);
		glUniform1f(glGetUniformLocation(shader, "max_v"), max_v);
		glUniform1f(glGetUniformLocation(shader, "run"), run);
		glUniform1f(glGetUniformLocation(shader, "threshold"), threshold);
		glUniform1f(glGetUniformLocation(shader, "threshold_run"), threshold_run);
		if(get_output()->get_color_model() != BC_YUV888 &&
			get_output()->get_color_model() != BC_YUVA8888)
			glUniform3f(glGetUniformLocation(shader, "key"),
				r_key, g_key, b_key);
		else
			glUniform3f(glGetUniformLocation(shader, "key"),
				(float)y_key / 0xff, (float)u_key / 0xff, (float)v_key / 0xff);
		if(config.use_value)
			BC_GL_RGB_TO_Y(shader);
	}
SET_TRACE

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	if(BC_CModels::components(get_output()->get_color_model()) == 3)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		get_output()->clear_pbuffer();
	}
SET_TRACE

	get_output()->draw_texture();

	glUseProgram(0);
	get_output()->set_opengl_state(VFrame::SCREEN);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glDisable(GL_BLEND);
SET_TRACE
#endif
	return 0;
}

