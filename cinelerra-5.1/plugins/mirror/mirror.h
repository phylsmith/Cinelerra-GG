
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

#ifndef MIRROR_H
#define MIRROR_H


class MirrorMain;

#include "filexml.h"
#include "mirrorwindow.h"
#include "guicast.h"
#include "pluginvclient.h"

class MirrorConfig
{
public:
	MirrorConfig();
	void reset(int clear);
	void copy_from(MirrorConfig &that);
	int equivalent(MirrorConfig &that);
	void interpolate(MirrorConfig &prev,
		MirrorConfig &next,
		int64_t prev_frame,
		int64_t next_frame,
		int64_t current_frame);
	int mirror_horizontal;
	int mirror_swaphorizontal;
	int mirror_vertical;
	int mirror_swapvertical;
	int reflection_center_enable;
	float reflection_center_x, reflection_center_y;
};

class MirrorMain : public PluginVClient
{
public:
	MirrorMain(PluginServer *server);
	~MirrorMain();

	PLUGIN_CLASS_MEMBERS(MirrorConfig);

// required for all realtime plugins
	int process_buffer(VFrame *frame,
		int64_t start_position,
		double frame_rate);
	int is_realtime();
	void update_gui();
	void save_data(KeyFrame *keyframe);
	void read_data(KeyFrame *keyframe);
	int handle_opengl();
};


#endif
