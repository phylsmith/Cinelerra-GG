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

#ifndef _ADCUTS_H_
#define _ADCUTS_H_

#include "adcuts.inc"
#include "bcwindowbase.inc"
#include "filexml.inc"
#include "file.inc"
#include "linklist.h"

class AdCut : public ListItem<AdCut>
{
public:
        double time;
	int action;

	AdCut(double t, int a) : time(t), action(a) {}
	~AdCut() {}
};

class AdCuts : public List<AdCut>
{
	int save(FileXML &xml);
	int load(FileXML &xml);
public:
	int fd, pid;
	char filename[BCTEXTLEN];

	void write_cuts(const char *filename);
	static AdCuts *read_cuts(const char *filename);

        AdCuts(int pid, int fd, const char *fn);
        ~AdCuts();
};

#endif
