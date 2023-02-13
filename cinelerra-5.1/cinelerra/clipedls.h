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


#ifndef __CLIP_EDLS_H__
#define __CLIP_EDLS_H__

#include "arraylist.h"
#include "edl.inc"

class ClipEDLs : public ArrayList<EDL*>
{
public:
	ClipEDLs();
	~ClipEDLs();

	void add_clip(EDL *clip);
	void remove_clip(EDL *clip);
// Return copy of the src EDL which belongs to the current object.
	EDL* get_nested(EDL *src);
	void copy_nested(ClipEDLs &nested);
	EDL* load(char *path);
	void clear();
	void update_index(EDL *clip);
};

#endif


