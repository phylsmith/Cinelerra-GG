
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

#include "bcdisplayinfo.h"
#include "bchash.h"
#include "filesystem.h"
#include "gainwindow.h"
#include "theme.h"
#include "language.h"

#include <string.h>







GainWindow::GainWindow(GainMain *plugin)
 : PluginClientWindow(plugin,
	xS(420),
	yS(60),
	xS(420),
	yS(60),
	0)
{
	this->plugin = plugin;
}

GainWindow::~GainWindow()
{
}

void GainWindow::create_objects()
{
	int xs10 = xS(10);
	int ys10 = yS(10);
	int x = xs10, y = ys10;
	int x2 = xS(80), x3 = xS(180);
	int clr_x = get_w()-x - xS(22); // note: clrBtn_w = 22

	y += ys10;
	add_tool(new BC_Title(x, y, _("Level:")));
	level_text = new GainLevelText(this, plugin, (x + x2), y);
	level_text->create_objects();
	add_tool(level_slider = new GainLevelSlider(this, plugin, x3, y));
	clr_x = x3 + level_slider->get_w() + x;
	add_subwindow(level_clr = new GainLevelClr(this, plugin, clr_x, y));
	show_window();
	flush();
}



void GainWindow::update()
{
	float level = plugin->config.level;
	level_text->update(level);
	level_slider->update(level);
}



GainLevelText::GainLevelText(GainWindow *window, GainMain *plugin, int x, int y)
 : BC_TumbleTextBox(window, plugin->config.level,
	(float)INFINITYGAIN, (float)GAINLEVEL_MAX, x, y, xS(60), 2)
{
	this->window = window;
	this->plugin = plugin;
	set_increment(0.1);
}
GainLevelText::~GainLevelText()
{
}
int GainLevelText::handle_event()
{
	float min = INFINITYGAIN, max = GAINLEVEL_MAX;
	float output = atof(get_text());

	if(output > max) output = max;
	else if(output < min) output = min;
	plugin->config.level = output;
	window->update();
	plugin->send_configure_change();
	return 1;
}



GainLevelSlider::GainLevelSlider(GainWindow *window, GainMain *plugin, int x, int y)
 : BC_FSlider(x, y, 0, xS(200),	yS(200), INFINITYGAIN, GAINLEVEL_MAX, plugin->config.level)
{
	this->window = window;
	this->plugin = plugin;
	enable_show_value(0); // Hide caption
}
GainLevelSlider::~GainLevelSlider()
{
}
int GainLevelSlider::handle_event()
{
	plugin->config.level = get_value();
	window->level_text->update((float)plugin->config.level);
	plugin->send_configure_change();
	return 1;
}



GainLevelClr::GainLevelClr(GainWindow *window, GainMain *plugin, int x, int y)
 : BC_Button(x, y, plugin->get_theme()->get_image_set("reset_button"))
{
	this->window = window;
	this->plugin = plugin;
}
GainLevelClr::~GainLevelClr()
{
}
int GainLevelClr::handle_event()
{
	plugin->config.reset();
	window->update();
	plugin->send_configure_change();
	return 1;
}
