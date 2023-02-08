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

#include "meterhistory.h"

#include <stdlib.h>
#include <math.h>

MeterHistory::MeterHistory()
{
	size = 0;
	channels = 0;
	current_peak = 0;
	samples = 0;
	values = 0;
}
MeterHistory::~MeterHistory()
{
	init(0, 0);
}

void MeterHistory::init(int chs, int sz)
{
	if( size != sz ) {
		delete [] samples;  samples = 0;
		size = 0;
	}
	if( !samples && sz > 0 ) {
		samples = new int64_t[size = sz];
		for( int i=0; i<size; ++i ) samples[i] = -1;
	}
	if( channels != chs ) {
		for( int i=0; i<channels; ++i ) delete [] values[i];
		delete [] values;  values = 0;
		delete [] current_peak;  current_peak = 0;
		channels = 0;
	}
	if( !values && chs > 0 ) {
		current_peak = new int[channels = chs];
		for( int i=0; i<channels; ++i ) current_peak[i] = 0;
		values = new double*[channels];
		for( int i=0; i<channels; ++i ) values[i] = 0;
	}
}

void MeterHistory::reset_channel(int ch)
{
	if( !ch ) for( int i=0; i<size; ++i ) samples[i] = -1;
	current_peak[ch] = 0;
	double *peaks = values[ch];
	if( !peaks ) values[ch] = peaks = new double[size];
	for( int i=0; i<size; ++i ) peaks[i] = 0;
}

void MeterHistory::set_peak(int ch, double peak, int64_t pos)
{
	int peak_idx = current_peak[ch];
	samples[peak_idx] = pos;
	values[ch][peak_idx++] = peak;
	if( peak_idx >= size ) peak_idx = 0;
	current_peak[ch] = peak_idx;
}

double MeterHistory::get_peak(int ch, int idx)
{
	return idx>=0 ? values[ch][idx] : 0;
}

int MeterHistory::get_nearest(int64_t pos, int64_t tolerance)
{
	int result = -1;
	if( size > 0 ) {
		int64_t best = tolerance;
		for( int i=0; i<size; ++i ) {
			int64_t diff = labs(samples[i] - pos);
			if( diff >= tolerance || diff >= best ) continue;
			best = diff;  result = i;
		}
	}
	return result;
}

