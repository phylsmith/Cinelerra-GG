
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

#include "bcdisplayinfo.h"
#include "mirrorwindow.h"
#include "language.h"
#include "theme.h"







MirrorWindow::MirrorWindow(MirrorMain *client)
 : PluginClientWindow(client, xS(420), yS(270), xS(420), yS(270), 0)
{
	this->client = client;
}

MirrorWindow::~MirrorWindow()
{
	delete mirror_horizontal;
	delete mirror_swaphorizontal;
	delete mirror_vertical;
	delete mirror_swapvertical;
	delete reflection_center_enable;
	delete reflection_center_x_text;
	delete reflection_center_x_slider;
	delete reflection_center_y_text;
	delete reflection_center_y_slider;
	delete reflection_center_x_clr;
	delete reflection_center_y_clr;
	delete reset;
}

void MirrorWindow::create_objects()
{
	int xs10 = xS(10), xs20 = xS(20), xs200 = xS(200);
	int ys10 = yS(10), ys20 = yS(20), ys30 = yS(30), ys40 = yS(40);
	int x2 = xS(60), x3 = xS(180);
	int x = xs10, y = ys10;
	int clr_x = get_w()-x - xS(22); // note: clrBtn_w = 22

	BC_TitleBar *title_bar;
	BC_Bar *bar;


// Direction section
	add_subwindow(title_bar = new BC_TitleBar(x, y, get_w()-2*x, xs20, xs10, _("Direction")));
	y += ys20;
	add_tool(mirror_horizontal = new MirrorToggle(client,
		&(client->config.mirror_horizontal),
		_("Horizontal"),
		x,
		y));
	add_tool(mirror_swaphorizontal = new MirrorSwapSide(client,
		&(client->config.mirror_swaphorizontal),
		_("Swap LEFT-RIGHT side"),
		(get_w() / 2),
		y));
	y += ys30;
	add_tool(mirror_vertical = new MirrorToggle(client,
		&(client->config.mirror_vertical),
		_("Vertical"),
		x,
		y));
	add_tool(mirror_swapvertical = new MirrorSwapSide(client,
		&(client->config.mirror_swapvertical),
		_("Swap TOP-BOTTOM side"),
		(get_w() / 2),
		y));
	y += ys40;
	add_tool(reflection_center_enable = new MirrorReflectionCenter(this, client, x, y));
	y += ys40;

// Reflection Center section
	add_subwindow(title_bar = new BC_TitleBar(x, y, get_w()-2*x, xs20, xs10, _("Reflection Center")));
	y += ys20;
	add_tool(new BC_Title(x, y, _("X")));
	add_tool(new BC_Title((x2-x), y, _("%")));
	reflection_center_x_text = new MirrorReflectionCenterXText(this, client, (x + x2), y);
	reflection_center_x_text->create_objects();
	reflection_center_x_slider = new MirrorReflectionCenterXSlider(this, client, x3, y, xs200);
	add_subwindow(reflection_center_x_slider);
	add_subwindow(reflection_center_x_clr = new MirrorReflectionCenterClr(this, client,
		clr_x, y, RESET_REFLECTION_CENTER_X));
	y += ys30;
	add_tool(new BC_Title(x, y, _("Y")));
	add_tool(new BC_Title((x2-x), y, _("%")));
	reflection_center_y_text = new MirrorReflectionCenterYText(this, client, (x + x2), y);
	reflection_center_y_text->create_objects();
	reflection_center_y_slider = new MirrorReflectionCenterYSlider(this, client, x3, y, xs200);
	add_subwindow(reflection_center_y_slider);
	add_subwindow(reflection_center_y_clr = new MirrorReflectionCenterClr(this, client,
		clr_x, y, RESET_REFLECTION_CENTER_Y));
	y += ys40;

// Reset section
	add_subwindow(bar = new BC_Bar(x, y, get_w()-2*x));
	y += ys10;
	add_tool(reset = new MirrorReset(client, this, x, y));
	show_window();
	flush();

// Needed to update Enable-Disable GUI when "Show controls" is pressed.
	update_reflection_center_enable();
}


void MirrorWindow::update_reflection_center_enable()
{
	if(client->config.reflection_center_enable)
	{
		reflection_center_x_text->enable();
		reflection_center_x_slider->enable();
		reflection_center_x_clr->enable();
		reflection_center_y_text->enable();
		reflection_center_y_slider->enable();
		reflection_center_y_clr->enable();
		mirror_swaphorizontal->disable();
		mirror_swaphorizontal->hide_window();
		mirror_swapvertical->disable();
		mirror_swapvertical->hide_window();
	}
	else
	{
		reflection_center_x_text->disable();
		reflection_center_x_slider->disable();
		reflection_center_x_clr->disable();
		reflection_center_y_text->disable();
		reflection_center_y_slider->disable();
		reflection_center_y_clr->disable();
		mirror_swaphorizontal->enable();
		mirror_swaphorizontal->show_window();
		mirror_swapvertical->enable();
		mirror_swapvertical->show_window();
	}
}

void MirrorWindow::update(int clear)
{
	switch(clear) {
		case RESET_REFLECTION_CENTER_X :
			reflection_center_x_text->update((float)client->config.reflection_center_x);
			reflection_center_x_slider->update((float)client->config.reflection_center_x);
			mirror_horizontal->update(client->config.mirror_horizontal);
			mirror_swaphorizontal->update(client->config.mirror_swaphorizontal);
			break;
		case RESET_REFLECTION_CENTER_Y :
			reflection_center_y_text->update((float)client->config.reflection_center_y);
			reflection_center_y_slider->update((float)client->config.reflection_center_y);
			mirror_vertical->update(client->config.mirror_vertical);
			mirror_swapvertical->update(client->config.mirror_swapvertical);
			break;
		case RESET_ALL :
		case RESET_DEFAULT_SETTINGS :
		default:
			mirror_horizontal->update(client->config.mirror_horizontal);
			mirror_swaphorizontal->update(client->config.mirror_swaphorizontal);
			mirror_vertical->update(client->config.mirror_vertical);
			mirror_swapvertical->update(client->config.mirror_swapvertical);
			reflection_center_enable->update(client->config.reflection_center_enable);
			reflection_center_x_text->update((float)client->config.reflection_center_x);
			reflection_center_x_slider->update((float)client->config.reflection_center_x);
			reflection_center_y_text->update((float)client->config.reflection_center_y);
			reflection_center_y_slider->update((float)client->config.reflection_center_y);
			break;
	}
}

MirrorToggle::MirrorToggle(MirrorMain *client, int *output, char *string, int x, int y)
 : BC_CheckBox(x, y, *output, string)
{
	this->client = client;
	this->output = output;
}
MirrorToggle::~MirrorToggle()
{
}
int MirrorToggle::handle_event()
{
	*output = get_value();
	client->send_configure_change();
	return 1;
}

MirrorSwapSide::MirrorSwapSide(MirrorMain *client, int *output, char *string, int x, int y)
 : BC_CheckBox(x, y, *output, string)
{
	this->client = client;
	this->output = output;
}
MirrorSwapSide::~MirrorSwapSide()
{
}
int MirrorSwapSide::handle_event()
{
	*output = get_value();
	client->send_configure_change();
	return 1;
}

MirrorReflectionCenter::MirrorReflectionCenter(MirrorWindow *window, MirrorMain *client, int x, int y)
 : BC_CheckBox(x, y, client->config.reflection_center_enable, _("Enable Reflection Center"))
{
        this->window = window;
	this->client = client;
}
MirrorReflectionCenter::~MirrorReflectionCenter()
{
}
int MirrorReflectionCenter::handle_event()
{
	client->config.reflection_center_enable = get_value();

	if(client->config.reflection_center_enable)
	{
		window->reflection_center_x_text->enable();
		window->reflection_center_x_slider->enable();
		window->reflection_center_x_clr->enable();
		window->reflection_center_y_text->enable();
		window->reflection_center_y_slider->enable();
		window->reflection_center_y_clr->enable();

		window->mirror_swaphorizontal->disable();
		window->mirror_swaphorizontal->hide_window();
		window->mirror_swapvertical->disable();
		window->mirror_swapvertical->hide_window();

		window->reflection_center_x_text->update(client->config.reflection_center_x);
		window->reflection_center_x_slider->update(client->config.reflection_center_x);
		window->reflection_center_y_text->update(client->config.reflection_center_y);
		window->reflection_center_y_slider->update(client->config.reflection_center_y);
		window->mirror_swaphorizontal->update(client->config.mirror_swaphorizontal);
		window->mirror_swapvertical->update(client->config.mirror_swapvertical);
	}
	else
	{
		window->reflection_center_x_text->disable();
		window->reflection_center_x_slider->disable();
		window->reflection_center_x_clr->disable();
		window->reflection_center_y_text->disable();
		window->reflection_center_y_slider->disable();
		window->reflection_center_y_clr->disable();

		window->mirror_swaphorizontal->enable();
		window->mirror_swaphorizontal->show_window();
		window->mirror_swapvertical->enable();
		window->mirror_swapvertical->show_window();

		window->reflection_center_x_text->update(client->config.reflection_center_x);
		window->reflection_center_x_slider->update(client->config.reflection_center_x);
		window->reflection_center_y_text->update(client->config.reflection_center_y);
		window->reflection_center_y_slider->update(client->config.reflection_center_y);
		window->mirror_swaphorizontal->update(client->config.mirror_swaphorizontal);
		window->mirror_swapvertical->update(client->config.mirror_swapvertical);
	}
	client->send_configure_change();
	return 1;
}

/* ********************************************** */
/* **** MIRROR REFLECTION CENTER X ************** */
MirrorReflectionCenterXText::MirrorReflectionCenterXText(MirrorWindow *window, MirrorMain *client,
	int x,
	int y)
 : BC_TumbleTextBox(window, client->config.reflection_center_x,
	(float)0.00, (float)100.00, x, y, xS(60), 2)
{
	this->window = window;
	this->client = client;
}

MirrorReflectionCenterXText::~MirrorReflectionCenterXText()
{
}

int MirrorReflectionCenterXText::handle_event()
{
	client->config.reflection_center_x = atof(get_text());
	if(client->config.reflection_center_x > 100.00) client->config.reflection_center_x = 100.00;
	else if(client->config.reflection_center_x < 0.00) client->config.reflection_center_x = 0.00;
	window->reflection_center_x_text->update(client->config.reflection_center_x);
	window->reflection_center_x_slider->update(client->config.reflection_center_x);
	client->send_configure_change();
	return 1;
}

MirrorReflectionCenterXSlider::MirrorReflectionCenterXSlider(MirrorWindow *window, MirrorMain *client,
	int x, int y, int w)
 : BC_FSlider(x, y, 0, w, w, 0.00, 100.00, client->config.reflection_center_x)
{
	this->window = window;
	this->client = client;
	enable_show_value(0); // Hide caption
	set_precision(0.01);
}

MirrorReflectionCenterXSlider::~MirrorReflectionCenterXSlider()
{
}

int MirrorReflectionCenterXSlider::handle_event()
{
	client->config.reflection_center_x = get_value();
	window->reflection_center_x_text->update(client->config.reflection_center_x);
	window->update(RESET_ALL);
	client->send_configure_change();
	return 1;
}
/* ********************************************** */

/* ********************************************** */
/* **** MIRROR REFLECTION CENTER Y ************** */
MirrorReflectionCenterYText::MirrorReflectionCenterYText(MirrorWindow *window, MirrorMain *client,
	int x,
	int y)
 : BC_TumbleTextBox(window, client->config.reflection_center_y,
	(float)0.00, (float)100.00, x, y, xS(60), 2)
{
	this->window = window;
	this->client = client;
}

MirrorReflectionCenterYText::~MirrorReflectionCenterYText()
{
}

int MirrorReflectionCenterYText::handle_event()
{
	client->config.reflection_center_y = atof(get_text());
	if(client->config.reflection_center_y > 100.00) client->config.reflection_center_y = 100.00;
	else if(client->config.reflection_center_y < 0.00) client->config.reflection_center_y = 0.00;
	window->reflection_center_y_text->update(client->config.reflection_center_y);
	window->reflection_center_y_slider->update(client->config.reflection_center_y);
	client->send_configure_change();
	return 1;
}

MirrorReflectionCenterYSlider::MirrorReflectionCenterYSlider(MirrorWindow *window, MirrorMain *client,
	int x, int y, int w)
 : BC_FSlider(x, y, 0, w, w, 0.00, 100.00, client->config.reflection_center_y)
{
	this->window = window;
	this->client = client;
	enable_show_value(0); // Hide caption
	set_precision(0.01);
}

MirrorReflectionCenterYSlider::~MirrorReflectionCenterYSlider()
{
}

int MirrorReflectionCenterYSlider::handle_event()
{
	client->config.reflection_center_y = get_value();
	window->reflection_center_y_text->update(client->config.reflection_center_y);
	window->update(RESET_ALL);
	client->send_configure_change();
	return 1;
}
/* ********************************************** */


MirrorReflectionCenterClr::MirrorReflectionCenterClr(MirrorWindow *window, MirrorMain *client, int x, int y, int clear)
 : BC_Button(x, y, client->get_theme()->get_image_set("reset_button"))
{
	this->window = window;
	this->client = client;
	this->clear = clear;
}
MirrorReflectionCenterClr::~MirrorReflectionCenterClr()
{
}
int MirrorReflectionCenterClr::handle_event()
{
	client->config.reset(clear);
	window->update(clear);
	client->send_configure_change();
	return 1;
}

MirrorReset::MirrorReset(MirrorMain *client, MirrorWindow *window, int x, int y)
 : BC_GenericButton(x, y, _("Reset"))
{
	this->client = client;
	this->window = window;
}

MirrorReset::~MirrorReset()
{
}

int MirrorReset::handle_event()
{
	client->config.reset(RESET_ALL);
	window->update_reflection_center_enable();
	window->update(RESET_ALL);
	client->send_configure_change();
	return 1;
}
