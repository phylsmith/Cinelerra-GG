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

#include "bcsignals.h"
#include "clipedls.h"
#include "edl.h"
#include "filexml.h"
#include "indexstate.h"


ClipEDLs::ClipEDLs()
{
}

ClipEDLs::~ClipEDLs()
{
	clear();
}

void ClipEDLs::clear()
{
	for( int i=0; i<size(); ++i ) get(i)->remove_user();
	remove_all();
}

void ClipEDLs::add_clip(EDL *edl)
{
	edl->folder_no = AW_CLIP_FOLDER;
	append(edl);
	edl->add_user();
}

void ClipEDLs::remove_clip(EDL *clip)
{
	int n = size();
	remove(clip);
	n -= size();
	while( --n >= 0 ) clip->remove_user();
}


EDL* ClipEDLs::get_nested(EDL *src)
{
	if( !src ) return 0;
	for( int i=0; i<size(); ++i ) {
		EDL *dst = get(i);
		if( src == dst || src->id == dst->id ) return dst;
	}
	for( int i=0; i<size(); ++i ) {
		EDL *dst = get(i);
		if( !strcmp(dst->path, src->path) ) return dst;
	}

	EDL *dst = new EDL;
	dst->create_objects();
	dst->copy_all(src);
	append(dst);
	return dst;
}

EDL* ClipEDLs::load(char *path)
{
	for( int i=0; i<size(); ++i ) {
		EDL *dst = get(i);
		if( !strcmp(dst->path, path) ) return dst;
	}

	EDL *dst = new EDL;
	dst->create_objects();

	FileXML xml_file;
	xml_file.read_from_file(path);
	dst->load_xml(&xml_file, LOAD_ALL);

// Override path EDL was saved to with the path it was loaded from.
	dst->set_path(path);
	append(dst);
	return dst;
}

void ClipEDLs::copy_nested(ClipEDLs &nested)
{
	clear();
	for( int i=0; i<nested.size(); ++i ) {
		EDL *new_edl = new EDL;
		new_edl->create_objects();
		new_edl->copy_all(nested[i]);
		append(new_edl);
	}
}

void ClipEDLs::update_index(EDL *clip_edl)
{
	for( int i=0; i<size(); ++i ) {
		EDL *current = get(i);
		if( !strcmp(current->path, clip_edl->path) ) {
			current->update_index(clip_edl);
		}
	}
}

