
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
#ifdef HAVE_LIBDPX

#ifndef FILEDPX_H
#define FILEDPX_H

#include "file.inc"
#include "filedpx.inc"
#include "filelist.h"
#include "DPX.h"

class FileDPX : public FileList
{
public:
	FileDPX(Asset *asset, File *file);
	~FileDPX();

	static void get_parameters(BC_WindowBase *parent_window,
		Asset *asset, BC_WindowBase* &format_window,
		int audio_options, int video_options, EDL *edl);
	static int check_sig(Asset *asset, char *test);
	static int get_best_colormodel(Asset *asset, int driver);
	int colormodel_supported(int colormodel);
	int read_frame_header(char *path);
	int read_frame(VFrame *frame, VFrame *data);

private:
	int color_model;
};

#endif
#endif
