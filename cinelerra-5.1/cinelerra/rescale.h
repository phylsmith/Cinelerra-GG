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

#ifndef __RESCALE_H__
#define __RESCALE_H__
#include "indexable.inc"

class Rescale {
public:
  	enum { none, scaled, cropped, filled, horiz_edge, vert_edge, n_scale_types };
	static const char *scale_types[];

	Rescale(int w, int h, double aspect);
	Rescale(Indexable *in);
	~Rescale();

	int w, h;
	double aspect;

	void rescale(Rescale &out, int type,
		float &src_w,float &src_h, float &dst_w,float &dst_h);
};

#endif
