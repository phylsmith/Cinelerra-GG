/*
 * CINELERRA
 * Copyright (C) 2016 Eric Olson
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef YUV411_H
#define YUV411_H


class yuv411Main;
class yuv411Window;

#include "filexml.h"
#include "yuv411win.h"
#include "guicast.h"
#include "pluginvclient.h"

class yuv411Config
{
public:
	yuv411Config();
	void reset();

	void copy_from(yuv411Config &that);
	int equivalent(yuv411Config &that);
	void interpolate(yuv411Config &prev, yuv411Config &next,
		long prev_frame, long next_frame, long current_frame);
	int int_horizontal;
	int avg_vertical;
	int inpainting;
	int offset;
	int thresh;
	int bias;
};

class yuv411Main : public PluginVClient
{
	VFrame *temp_frame;
	int colormodel;
	friend class yuv411Window;
public:
	yuv411Main(PluginServer *server);
	~yuv411Main();

// required for all realtime plugins
	PLUGIN_CLASS_MEMBERS(yuv411Config)

	int process_realtime(VFrame *input_ptr, VFrame *output_ptr);
	int is_realtime();
	void update_gui();
	void render_gui(void *data);
	void save_data(KeyFrame *keyframe);
	void read_data(KeyFrame *keyframe);
};


#endif
