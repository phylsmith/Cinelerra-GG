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

#ifndef __PLUGINFCLIENT_INC__
#define __PLUGINFCLIENT_INC__

struct AVFilter;
typedef struct AVFilter AVFilter;
struct AVFilterGraph;
typedef struct AVFilterGraph AVFilterGraph;

class PluginFClient_Opt;
class PluginFFilter;
class PluginFClientConfig;
class PluginFClient_OptName;
class PluginFClient_OptValue;
class PluginFClient_OptPanel;
class PluginFClientUnits;
class PluginFClientText;
class PluginFClientApply;
class PluginFClientWindow;
class PluginFClient;
class PluginFAClient;
class PluginFVClient;

#ifndef  FLT_MAX
#define FLT_MAX 3.40282346638528859812e+38F
#endif

#ifndef  FLT_MIN
#define FLT_MIN 1.17549435082228750797e-38F
#endif

#ifndef  DBL_MAX
#define DBL_MAX ((double)1.79769313486231570815e+308L)
#endif

#ifndef  DBL_MIN
#define DBL_MIN ((double)2.22507385850720138309e-308L)
#endif

#endif
