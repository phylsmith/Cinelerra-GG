
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

#include "asset.h"
#include "file.h"
#include "filedpx.h"

class DPXInStream : public InStream
{
public:
	DPXInStream(char * ptr, size_t sz);
	virtual ~DPXInStream();

	void Close();
	void Rewind();
	size_t Read(void * buf, const size_t size);
	size_t ReadDirect(void * buf, const size_t size);
	bool EndOfFile() const;
	bool Seek(long offset, Origin origin);

private:
	char *databuf;
	size_t pos;
	size_t bufsize;
};

DPXInStream::DPXInStream(char * ptr, size_t sz) :
	databuf(ptr),
	pos(0),
	bufsize(sz)
{
}

DPXInStream::~DPXInStream()
{
	Close();
}

void DPXInStream::Close()
{
	databuf = nullptr;
	bufsize = 0;
	pos = 0;
}

void DPXInStream::Rewind()
{
	pos = 0;
}

size_t DPXInStream::Read(void * buf, const size_t size)
{
	size_t data_to_read = MIN(size, bufsize - pos);
	if ( data_to_read > 0 )
	{
		memcpy(buf, &databuf[pos], data_to_read);
		pos += data_to_read;
	}
	return data_to_read;
}

size_t DPXInStream::ReadDirect(void * buf, const size_t size)
{
	this->Read(buf, size);
}

bool DPXInStream::EndOfFile() const
{
	if ( pos >= bufsize )
		return true;
	return false;
}

bool DPXInStream::Seek(long offset, Origin origin)
{
	bool result = true;
	switch ( origin )
	{
	case kStart:
		if ( (size_t)offset < bufsize )
			pos = offset;
		else
			result = false;
	break;

	case kCurrent:
		if ( pos+offset < bufsize )
			pos += offset;
		else
			result = false;
	break;

	case kEnd:
		if ( (size_t)offset < bufsize )
			pos = bufsize - offset - 1;
		else
			result = false;
	break;
	}
	return result;
}




FileDPX::FileDPX(Asset *asset, File *file)
 : FileList(asset, file, "DPXLIST", ".dpx", FILE_DPX, FILE_DPX_LIST)
{
	if(asset->format == FILE_UNKNOWN) 
		asset->format = FILE_DPX_LIST;
}

FileDPX::~FileDPX()
{
}

void FileDPX::get_parameters(BC_WindowBase *parent_window,
	Asset *asset, BC_WindowBase* &format_window,
	int audio_options, int video_options, EDL *edl)
{
}

int FileDPX::check_sig(Asset *asset, char *test)
{
	if(test[0] == 'D' && test[1] == 'P' && test[2] == 'X' &&
		test[3] == 'L' && test[4] == 'I' && test[5] == 'S' && test[6] == 'T')
	{
		return 1;
	}
	return 0;
}

int FileDPX::get_best_colormodel(Asset *asset, int driver)
{
	return BC_RGB161616;
}

int FileDPX::colormodel_supported(int colormodel)
{
	return color_model;
}

int FileDPX::read_frame_header(char *path)
{
	int result = 0;

	InStream img;
	if (!img.Open(path))
	{
		return 1;
	}
	
	dpx::Header header;	
	if (!header.Read(&img))
	{		
		return 1;
	}
		
	asset->width = header.Width();
	asset->height = header.Height();
	switch ( header.ComponentDataSize(0) )
	{
		case dpx::DataSize::kByte:
			color_model = BC_RGB888;
			break;
		
		case dpx::DataSize::kWord:
			color_model = BC_RGB161616;
			break;

		case dpx::DataSize::kInt:
		case dpx::DataSize::kFloat:
		case dpx::DataSize::kDouble:
			color_model = BC_RGB_FLOAT;
			break;
	}
	return result;
}

int FileDPX::read_frame(VFrame *frame, VFrame *data)
{	
	DPXInStream inStream((char*)data->get_data(), data->get_compressed_size());
	dpx::Reader dpxReader;

	dpxReader.SetInStream(&inStream);
	dpxReader.ReadHeader();
	return dpxReader.ReadImage(0, frame->get_data()) ? 0 : 1;
}

#endif