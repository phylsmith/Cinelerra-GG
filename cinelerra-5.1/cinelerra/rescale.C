/*
 * CINELERRA
 * Copyright (C) 2016-2020 William Morrow
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

#include "rescale.h"
#include "indexable.h"
#include "language.h"
#include "mwindow.h"

const char *Rescale::scale_types[] = {
  N_("None"), N_("Scaled"), N_("Cropped"), N_("Filled"), N_("Horiz Edge"), N_("Vert Edge"),
};

Rescale::Rescale(int w, int h, double aspect)
{
	this->w = w;
	this->h = h;
	this->aspect = aspect;
}

Rescale::Rescale(Indexable *in)
{
	this->w = in->get_w();
	this->h = in->get_h();
	float aw, ah;
	MWindow::create_aspect_ratio(aw, ah, this->w, this->h);
	this->aspect = ah > 0 ? aw / ah : 1;
}

Rescale::~Rescale()
{
}

void Rescale::rescale(Rescale &out, int type,
		float &src_w,float &src_h, float &dst_w,float &dst_h)
{
	int in_w = w, in_h = h;
	int out_w = out.w, out_h = out.h;

	src_w = in_w;  src_h = in_h;
	dst_w = out_w; dst_h = out_h;
	double r = out.aspect / aspect;

	switch( type ) {
	case cropped:
		if( r < 1 )
			src_w = in_w * r;
		else
			src_h = in_h / r;
		break;
	case filled:
		if( r < 1 )
			dst_h = out_h * r;
		else
			dst_w = out_w / r;
		break;
	case horiz_edge:
		if( r < 1 )
			dst_h = out_h * r;
		else
			src_h = in_h / r;
		break;
	case vert_edge:
		if( r < 1 )
			src_w = in_w * r;
		else
			dst_w = out_w / r;
		break;
	default:
		break;
	}
}

