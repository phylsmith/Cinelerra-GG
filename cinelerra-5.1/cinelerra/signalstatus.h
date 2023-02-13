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

#ifndef _SIGNALSTATUS_H_
#define _SIGNALSTATUS_H_
#ifdef HAVE_DVB

#include "bcwindowbase.h"
#include "bcsubwindow.h"
#include "bctitle.inc"
#include "channel.inc"
#include "devicedvbinput.inc"


class SignalStatus : public BC_SubWindow
{
	friend class DeviceDVBInput;
	enum {  pad0 = 3, pad1 = 8,
		lck_w = 5,  lck_h = 5,
		crr_w = 5,  crr_h = 5,
        	pwr_w = 32, pwr_h = 4,
        	snr_w = 32, snr_h = 4,
		ber_w = 32, ber_h = 4,
		unc_w = 32, unc_h = 4,
	};

	DeviceDVBInput *dvb_input;
	BC_Title *channel_title;
        int lck_x, lck_y;
        int crr_x, crr_y;
        int pwr_x, pwr_y;
        int snr_x, snr_y;
        int ber_x, ber_y;
        int unc_x, unc_y;

	void update_lck(int v);
	void update_crr(int v);
	void update_pwr(int v);
	void update_snr(int v);
	void update_ber(int v);
	void update_unc(int v);
public:
	BC_WindowBase *wdw;

	void create_objects();
	static int calculate_w(BC_WindowBase *wdw);
	static int calculate_h(BC_WindowBase *wdw);
	void disconnect() { dvb_input = 0; }
	void update();

	SignalStatus(BC_WindowBase *wdw, int x, int y);
	~SignalStatus();
};

#endif
#endif
