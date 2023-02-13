/*
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


#ifndef __PLUGINLV2CLIENT_H__
#define __PLUGINLV2CLIENT_H__

#include "condition.inc"
#include "mutex.h"
#include "pluginaclient.h"
#include "plugin.inc"
#include "pluginlv2.h"
#include "pluginlv2config.h"
#include "pluginlv2client.inc"
#include "pluginlv2gui.h"
#include "pluginlv2ui.inc"
#include "samples.inc"

class PluginLV2UIs : public ArrayList<PluginLV2ParentUI *>, public Mutex
{
public:
	PluginLV2UIs();
	~PluginLV2UIs();

	void del_uis();
	PluginLV2ParentUI *del_ui(PluginLV2Client *client);
	PluginLV2ParentUI *del_ui(PluginLV2ClientWindow *gui);
	PluginLV2ParentUI *add_ui(PluginLV2ParentUI *ui, PluginLV2Client *client);
	PluginLV2ParentUI *search_ui(int plugin_id);
	PluginLV2ParentUI *find_ui(int plugin_id);
	PluginLV2ParentUI *get_ui(PluginLV2Client *client);

};

class PluginLV2ParentUI : public ForkParent
{
public:
	PluginLV2ParentUI(int plugin_id);
	~PluginLV2ParentUI();
	ForkChild* new_fork();
	void start_parent(PluginLV2Client *client);
	int handle_parent();

	Condition *output_bfr;
	PluginLV2Client *client;
	PluginLV2ClientWindow *gui;

	int plugin_id;
	int hidden;
	int show();
	int hide();

	static PluginLV2UIs plugin_lv2;
};

class PluginLV2BlackList : public ArrayList<const char *>
{
public:
	PluginLV2BlackList(const char *path);
	~PluginLV2BlackList();

	int is_badboy(const char *uri);
};

class PluginLV2Client : public PluginAClient, public PluginLV2
{
public:
	PluginLV2Client(PluginServer *server);
	~PluginLV2Client();

	int process_realtime(int64_t size,
		Samples **input_ptr, Samples **output_ptr, int chs);
	int process_realtime(int64_t size,
		Samples *input_ptr, Samples *output_ptr);
	int process_realtime(int64_t size,
		Samples **input_ptr, Samples **output_ptr);
// Update output pointers as well
	int is_realtime();
	int is_multichannel();
	int is_synthesis();
	int uses_gui();
	char* to_string(char *string, const char *input);
	void save_data(KeyFrame *keyframe);
	void read_data(KeyFrame *keyframe);
	void load_buffer(int samples, Samples **input, int ich);
	int unload_buffer(int samples, Samples **output, int och);
	void process_buffer(int size);
	void update_gui();
	void update_lv2(int token);
	int init_lv2();
	PluginLV2ParentUI *find_ui();
	PluginLV2ParentUI *get_ui();

	PLUGIN_CLASS_MEMBERS(PluginLV2ClientConfig)
	char title[BCSTRLEN];
};

#endif
