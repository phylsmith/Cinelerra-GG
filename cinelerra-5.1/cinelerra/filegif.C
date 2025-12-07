
/*
 * CINELERRA
 * Copyright (C) 2014 Adam Williams <broadcast at earthling dot net>
 * Copyright (C) 2003-2016 Cinelerra CV contributors
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
#ifdef HAVE_GIFLIB
#include "asset.h"
#include "bcsignals.h"
#include "file.h"
#include "filegif.h"
#include "gif_lib.h"
#include "mainerror.h"
#include "vframe.h"

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

//from "getarg.h"
extern "C"
#if !defined HAVE_GIFLIB_QUANTIZE
int GifQuantizeBuffer(unsigned int Width, unsigned int Height,
                   int *ColorMapSize, const GifByteType * RedInput,
                   const GifByteType * GreenInput, const GifByteType * BlueInput,
                   GifByteType * OutputBuffer,
                   GifColorType * OutputColorMap);
#endif
#if GIFLIB_MAJOR == 5 && GIFLIB_MINOR >= 2 || GIFLIB_MAJOR == 5 && GIFLIB_MINOR == 1 && GIFLIB_RELEASE >= 9

#define ABS(x)    ((x) > 0 ? (x) : (-(x)))

#define COLOR_ARRAY_SIZE 32768
#define BITS_PER_PRIM_COLOR 5
#define MAX_PRIM_COLOR      0x1f

typedef struct QuantizedColorType {
    GifByteType RGB[3];
    GifByteType NewColorIndex;
    long Count;
    struct QuantizedColorType *Pnext;
} QuantizedColorType;

static int QCmpr(QuantizedColorType *a, QuantizedColorType *b, int i)
{
	int i0 = i, i1 = i+1, i2 = i+2;
	if( i1 >= 3 ) i1 -= 3;
	if( i2 >= 3 ) i2 -= 3;
	/* sort on all axes of the color space! */
	int hash_a = (a->RGB[i0] << 16) | (a->RGB[i1] << 8) | (a->RGB[i2] << 0);
	int hash_b = (b->RGB[i0] << 16) | (b->RGB[i1] << 8) | (b->RGB[i2] << 0);
	return hash_a - hash_b;
}

static int QSplit(QuantizedColorType **q, int l, int r, int i)
{
	int m;
	QuantizedColorType *t;
	for(;;) {
		while( QCmpr(q[r],q[l], i) >= 0 ) if( ++l == r ) return r;
		t = q[l];  q[l] = q[r];  q[r] = t;  m = l;  l = r;  r = m;
		while( QCmpr(q[l],q[r], i) >= 0 ) if( r == --l ) return r;
		t = q[l];  q[l] = q[r];  q[r] = t;  m = l;  l = r;  r = m;
	}
}

static void QSort(QuantizedColorType **q, int ll, int rr, int i)
{
	for(;;) {
		int l = ll+1;  if( l == rr ) return;
		int r = rr-1;  if( l == r ) return;
		int m = QSplit(q, l, r, i);
		QSort(q, ll, m, i);
		ll = m;
	}
}

typedef struct NewColorMapType {
    GifByteType RGBMin[3], RGBWidth[3];
    unsigned int NumEntries; /* # of QuantizedColorType in linked list below */
    unsigned long Count; /* Total number of pixels in all the entries */
    QuantizedColorType *QuantizedColors;
} NewColorMapType;

static int SubdivColorMap(NewColorMapType * NewColorSubdiv,
                          unsigned int ColorMapSize,
                          unsigned int *NewColorMapSize);

#if !defined HAVE_GIFLIB_QUANTIZE

/******************************************************************************
 Quantize high resolution image into lower one. Input image consists of a
 2D array for each of the RGB colors with size Width by Height. There is no
 Color map for the input. Output is a quantized image with 2D array of
 indexes into the output color map.
   Note input image can be 24 bits at the most (8 for red/green/blue) and
 the output has 256 colors at the most (256 entries in the color map.).
 ColorMapSize specifies size of color map up to 256 and will be updated to
 real size before returning.
   Also non of the parameter are allocated by this routine.
   This function returns GIF_OK if successful, GIF_ERROR otherwise.
******************************************************************************/
int
GifQuantizeBuffer(unsigned int Width,
               unsigned int Height,
               int *ColorMapSize,
               GifByteType * RedInput,
               GifByteType * GreenInput,
               GifByteType * BlueInput,
               GifByteType * OutputBuffer,
               GifColorType * OutputColorMap) {

    unsigned int Index, NumOfEntries;
    int i, j, MaxRGBError[3];
    unsigned int NewColorMapSize;
    long Red, Green, Blue;
    NewColorMapType NewColorSubdiv[256];
    QuantizedColorType *ColorArrayEntries, *QuantizedColor;

    ColorArrayEntries = (QuantizedColorType *)malloc(
                           sizeof(QuantizedColorType) * COLOR_ARRAY_SIZE);
    if (ColorArrayEntries == NULL) {
        return GIF_ERROR;
    }

    for (i = 0; i < COLOR_ARRAY_SIZE; i++) {
        ColorArrayEntries[i].RGB[0] = i >> (2 * BITS_PER_PRIM_COLOR);
        ColorArrayEntries[i].RGB[1] = (i >> BITS_PER_PRIM_COLOR) &
           MAX_PRIM_COLOR;
        ColorArrayEntries[i].RGB[2] = i & MAX_PRIM_COLOR;
        ColorArrayEntries[i].Count = 0;
    }

    /* Sample the colors and their distribution: */
    for (i = 0; i < (int)(Width * Height); i++) {
        Index = ((RedInput[i] >> (8 - BITS_PER_PRIM_COLOR)) <<
                  (2 * BITS_PER_PRIM_COLOR)) +
                ((GreenInput[i] >> (8 - BITS_PER_PRIM_COLOR)) <<
                  BITS_PER_PRIM_COLOR) +
                (BlueInput[i] >> (8 - BITS_PER_PRIM_COLOR));
        ColorArrayEntries[Index].Count++;
    }

    /* Put all the colors in the first entry of the color map, and call the
     * recursive subdivision process.  */
    for (i = 0; i < 256; i++) {
        NewColorSubdiv[i].QuantizedColors = NULL;
        NewColorSubdiv[i].Count = NewColorSubdiv[i].NumEntries = 0;
        for (j = 0; j < 3; j++) {
            NewColorSubdiv[i].RGBMin[j] = 0;
            NewColorSubdiv[i].RGBWidth[j] = 255;
        }
    }

    /* Find the non empty entries in the color table and chain them: */
    for (i = 0; i < COLOR_ARRAY_SIZE; i++)
        if (ColorArrayEntries[i].Count > 0)
            break;
    QuantizedColor = NewColorSubdiv[0].QuantizedColors = &ColorArrayEntries[i];
    NumOfEntries = 1;
    while (++i < COLOR_ARRAY_SIZE)
        if (ColorArrayEntries[i].Count > 0) {
            QuantizedColor->Pnext = &ColorArrayEntries[i];
            QuantizedColor = &ColorArrayEntries[i];
            NumOfEntries++;
        }
    QuantizedColor->Pnext = NULL;

    NewColorSubdiv[0].NumEntries = NumOfEntries; /* Different sampled colors */
    NewColorSubdiv[0].Count = ((long)Width) * Height; /* Pixels */
    NewColorMapSize = 1;
    if (SubdivColorMap(NewColorSubdiv, *ColorMapSize, &NewColorMapSize) !=
       GIF_OK) {
        free((char *)ColorArrayEntries);
        return GIF_ERROR;
    }
    if (NewColorMapSize < *ColorMapSize) {
        /* And clear rest of color map: */
        for (i = NewColorMapSize; i < *ColorMapSize; i++)
            OutputColorMap[i].Red = OutputColorMap[i].Green =
                OutputColorMap[i].Blue = 0;
    }

    /* Average the colors in each entry to be the color to be used in the
     * output color map, and plug it into the output color map itself. */
    for (i = 0; i < NewColorMapSize; i++) {
        if ((j = NewColorSubdiv[i].NumEntries) > 0) {
            QuantizedColor = NewColorSubdiv[i].QuantizedColors;
            Red = Green = Blue = 0;
            while (QuantizedColor) {
                QuantizedColor->NewColorIndex = i;
                Red += QuantizedColor->RGB[0];
                Green += QuantizedColor->RGB[1];
                Blue += QuantizedColor->RGB[2];
                QuantizedColor = QuantizedColor->Pnext;
            }
            OutputColorMap[i].Red = (Red << (8 - BITS_PER_PRIM_COLOR)) / j;
            OutputColorMap[i].Green = (Green << (8 - BITS_PER_PRIM_COLOR)) / j;
            OutputColorMap[i].Blue = (Blue << (8 - BITS_PER_PRIM_COLOR)) / j;
        }
    }

    /* Finally scan the input buffer again and put the mapped index in the
     * output buffer.  */
    MaxRGBError[0] = MaxRGBError[1] = MaxRGBError[2] = 0;
    for (i = 0; i < (int)(Width * Height); i++) {
        Index = ((RedInput[i] >> (8 - BITS_PER_PRIM_COLOR)) <<
                 (2 * BITS_PER_PRIM_COLOR)) +
                ((GreenInput[i] >> (8 - BITS_PER_PRIM_COLOR)) <<
                 BITS_PER_PRIM_COLOR) +
                (BlueInput[i] >> (8 - BITS_PER_PRIM_COLOR));
        Index = ColorArrayEntries[Index].NewColorIndex;
        OutputBuffer[i] = Index;
        if (MaxRGBError[0] < ABS(OutputColorMap[Index].Red - RedInput[i]))
            MaxRGBError[0] = ABS(OutputColorMap[Index].Red - RedInput[i]);
        if (MaxRGBError[1] < ABS(OutputColorMap[Index].Green - GreenInput[i]))
            MaxRGBError[1] = ABS(OutputColorMap[Index].Green - GreenInput[i]);
        if (MaxRGBError[2] < ABS(OutputColorMap[Index].Blue - BlueInput[i]))
            MaxRGBError[2] = ABS(OutputColorMap[Index].Blue - BlueInput[i]);
    }


#ifdef DEBUG
    fprintf(stderr,
            "Quantization L(0) errors: Red = %d, Green = %d, Blue = %d.\n",
            MaxRGBError[0], MaxRGBError[1], MaxRGBError[2]);
#endif /* DEBUG */

    free((char *)ColorArrayEntries);

    *ColorMapSize = NewColorMapSize;

    return GIF_OK;
}

#endif // HAVE_GIFLIB_QUANTIZE

/******************************************************************************
 Routine to subdivide the RGB space recursively using median cut in each
 axes alternatingly until ColorMapSize different cubes exists.
 The biggest cube in one dimension is subdivide unless it has only one entry.
 Returns GIF_ERROR if failed, otherwise GIF_OK.
*******************************************************************************/
static int
SubdivColorMap(NewColorMapType * NewColorSubdiv,
               unsigned int ColorMapSize,
               unsigned int *NewColorMapSize) {

    int SortRGBAxis = 0;
    unsigned int i, j, Index = 0;
    QuantizedColorType *QuantizedColor, **SortArray;

    while (ColorMapSize > *NewColorMapSize) {
        /* Find candidate for subdivision: */
	long Sum, Count;
        int MaxSize = -1;
	unsigned int NumEntries, MinColor, MaxColor;
        for (i = 0; i < *NewColorMapSize; i++) {
            for (j = 0; j < 3; j++) {
                if ((((int)NewColorSubdiv[i].RGBWidth[j]) > MaxSize) &&
                      (NewColorSubdiv[i].NumEntries > 1)) {
                    MaxSize = NewColorSubdiv[i].RGBWidth[j];
                    Index = i;
                    SortRGBAxis = j;
                }
            }
        }

        if (MaxSize == -1)
            return GIF_OK;

        /* Split the entry Index into two along the axis SortRGBAxis: */

        /* Sort all elements in that entry along the given axis and split at
         * the median.  */
        SortArray = (QuantizedColorType **)malloc(
                      sizeof(QuantizedColorType *) *
                      NewColorSubdiv[Index].NumEntries);
        if (SortArray == NULL)
            return GIF_ERROR;
        for (j = 0, QuantizedColor = NewColorSubdiv[Index].QuantizedColors;
             j < NewColorSubdiv[Index].NumEntries && QuantizedColor != NULL;
             j++, QuantizedColor = QuantizedColor->Pnext)
            SortArray[j] = QuantizedColor;

        QSort(SortArray, -1, NewColorSubdiv[Index].NumEntries, SortRGBAxis);

        /* Relink the sorted list into one: */
        for (j = 0; j < NewColorSubdiv[Index].NumEntries - 1; j++)
            SortArray[j]->Pnext = SortArray[j + 1];
        SortArray[NewColorSubdiv[Index].NumEntries - 1]->Pnext = NULL;
        NewColorSubdiv[Index].QuantizedColors = QuantizedColor = SortArray[0];
        free((char *)SortArray);

        /* Now simply add the Counts until we have half of the Count: */
        Sum = NewColorSubdiv[Index].Count / 2 - QuantizedColor->Count;
        NumEntries = 1;
        Count = QuantizedColor->Count;
        while (QuantizedColor->Pnext != NULL &&
	       (Sum -= QuantizedColor->Pnext->Count) >= 0 &&
               QuantizedColor->Pnext->Pnext != NULL) {
            QuantizedColor = QuantizedColor->Pnext;
            NumEntries++;
            Count += QuantizedColor->Count;
        }
        /* Save the values of the last color of the first half, and first
         * of the second half so we can update the Bounding Boxes later.
         * Also as the colors are quantized and the BBoxes are full 0..255,
         * they need to be rescaled.
         */
        MaxColor = QuantizedColor->RGB[SortRGBAxis]; /* Max. of first half */
	/* coverity[var_deref_op] */
        MinColor = QuantizedColor->Pnext->RGB[SortRGBAxis]; /* of second */
        MaxColor <<= (8 - BITS_PER_PRIM_COLOR);
        MinColor <<= (8 - BITS_PER_PRIM_COLOR);

        /* Partition right here: */
        NewColorSubdiv[*NewColorMapSize].QuantizedColors =
           QuantizedColor->Pnext;
        QuantizedColor->Pnext = NULL;
        NewColorSubdiv[*NewColorMapSize].Count = Count;
        NewColorSubdiv[Index].Count -= Count;
        NewColorSubdiv[*NewColorMapSize].NumEntries =
           NewColorSubdiv[Index].NumEntries - NumEntries;
        NewColorSubdiv[Index].NumEntries = NumEntries;
        for (j = 0; j < 3; j++) {
            NewColorSubdiv[*NewColorMapSize].RGBMin[j] =
               NewColorSubdiv[Index].RGBMin[j];
            NewColorSubdiv[*NewColorMapSize].RGBWidth[j] =
               NewColorSubdiv[Index].RGBWidth[j];
        }
        NewColorSubdiv[*NewColorMapSize].RGBWidth[SortRGBAxis] =
           NewColorSubdiv[*NewColorMapSize].RGBMin[SortRGBAxis] +
           NewColorSubdiv[*NewColorMapSize].RGBWidth[SortRGBAxis] - MinColor;
        NewColorSubdiv[*NewColorMapSize].RGBMin[SortRGBAxis] = MinColor;

        NewColorSubdiv[Index].RGBWidth[SortRGBAxis] =
           MaxColor - NewColorSubdiv[Index].RGBMin[SortRGBAxis];

        (*NewColorMapSize)++;
    }

    return GIF_OK;
}

#endif


FileGIF::FileGIF(Asset *asset, File *file)
 : FileBase(asset, file)
{
	offset = 0;
	err = 0;
	gif_file = 0;
	eof = 1;
	row_size = 0;
	rows = 0;
	depth = 8;
	fp = 0;
	fd = -1;
	writes = -1;
	buffer = 0;
	bg = 0;
	output = 0;
}

FileGIF::~FileGIF()
{
	close_file();
}

int FileGIF::check_sig(Asset *asset)
{
	FILE *stream = fopen(asset->path, "rb");
	if( stream ) {
		char test[8];
		int ret = fread(test, 1, 6, stream);
		fclose(stream);
		if( ret >= 6 &&
		    test[0] == 'G' && test[1] == 'I' && test[2] == 'F' &&
		    test[3] == '8' && (test[4] == '7' || test[4] == '9') &&
		    test[5] == 'a' ) return 1;
	}
	return 0;
}

int FileGIF::colormodel_supported(int colormodel)
{
	return BC_RGB888;
}

int FileGIF::get_best_colormodel(Asset *asset, int driver)
{
	return BC_RGB888;
}


int FileGIF::read_frame_header(char *path)
{
	FILE *stream = fopen(path, "rb");
	if( stream ) {
		unsigned char test[16];
		int ret = fread(test, 16, 1, stream);
		fclose(stream);
		if( ret < 1 ) return 1;
		asset->format = FILE_GIF;
		asset->width = test[6] | (test[7] << 8);
		asset->height = test[8] | (test[9] << 8);
		return 0;
	}

	perror(path);
	return 1;
}

static int input_file(GifFileType *gif_file, GifByteType *buffer, int bytes)
{
	FileGIF *file = (FileGIF*)gif_file->UserData;
	fseek(file->fp, file->offset, SEEK_SET);
	bytes = fread(buffer, 1, bytes, file->fp);
	file->offset += bytes;
	return bytes;
}

int FileGIF::open_file(int rd, int wr)
{
	return  rd ? ropen_path(asset->path) :
		wr ? wopen_path(asset->path) :
		0 ;
}

int FileGIF::ropen_path(const char *path)
{
	fp = fopen(path, "r");
	int result = !fp ? 1 : 0;
	if( !result ) {
		offset = 0;  eof = 0;
		gif_file = DGifOpen(this, input_file, &err);
		if( !gif_file ) {
			eprintf("FileGIF::ropen_path %d: %s\n", __LINE__, GifErrorString(err));
			result = 1;
		}
	}
	if( !result )
		result = open_gif();
	return result;
}

int FileGIF::wopen_path(const char *path)
{
	fd = open(path, O_CREAT+O_TRUNC+O_WRONLY, 0777);
	int result = fd < 0 ? 1 : 0;
	if( !result ) {
		gif_file = EGifOpenFileHandle(fd, &err);
		if( !gif_file ) {
			eprintf("FileGIF::wopen_path %d: %s\n", __LINE__, GifErrorString(err));
			result = 1;
		}
	}
	if( !result ) {
		writes = 0;
	}
	return result;
}

int FileGIF::write_frames(VFrame ***frames, int len)
{
	int result = !gif_file ? 1 : 0;
	for( int i=0; i<len && !result; ++i )
		result = write_frame(frames[0][i]);
	return result;
}

int FileGIF::open_gif()
{
	file_pos.remove_all();
	int width = asset->width;
	int height = asset->height;
	int result = read_frame_header(asset->path);
	if( !result ) {
		asset->actual_width = asset->width;
		if( width ) asset->width = width;
		asset->actual_height = asset->height;
		if( height ) asset->height = height;
		asset->layers = 1;
		if( !asset->frame_rate )
			asset->frame_rate = 10;
		asset->video_data = 1;
		row_size = gif_file->SWidth * sizeof(GifPixelType);
		bg = (GifRowType)malloc(row_size);
		for( int i=0; i<gif_file->SWidth; ++i )
			bg[i] = gif_file->SBackGroundColor;
		rows = gif_file->SHeight;
		buffer = (GifRowType*)malloc(sizeof(GifRowType) * rows);
		for( int i=0; i<gif_file->SHeight; ++i ) {
			buffer[i] = (GifRowType)malloc(row_size);
			memcpy(buffer[i], bg, row_size);
		}
		result = scan_gif();
		asset->video_length = file_pos.size();
	}
	return result;
}

int FileGIF::close_file()
{
	if( gif_file ) {
		EGifCloseFile(gif_file, &err);
		gif_file = 0;
	}
	if( fp ) {
		fclose(fp);  fp = 0;
	}
	if( fd >= 0 ) {
		close(fd);  fd = -1;
	}
	if( bg ) { free(bg);  bg = 0; }
	if( buffer ) {
		for( int k=0; k<rows; ++k )
			free(buffer[k]);
		free(buffer);  buffer = 0;
		rows = 0;
	}
	offset = 0;
	row_size = 0;
	err = 0;
	writes = -1;
	eof = 1;
	output = 0;
	FileBase::close_file();
	return 0;
}

int FileGIF::scan_gif()
{
	int file_eof = eof;
	int64_t file_offset = offset;
	file_pos.remove_all();
	int image_pos = offset, ret;
// read all imgs, build file_pos index
	while( (ret=read_next_image(0)) > 0 ) {
		file_pos.append(image_pos);
		image_pos = offset;
	}
	eof = file_eof;
	offset = file_offset;
	return ret;
}

int FileGIF::set_video_position(int64_t pos)
{
	if( !gif_file || !asset->video_length ) return 1;
	int64_t sz = file_pos.size();
	eof = pos < 0 || pos >= sz ? 1 : 0;
	offset = !eof ? file_pos[pos] : 0;
	return 0;
}

int FileGIF::read_frame(VFrame *output)
{
	if( !gif_file ) return 1;
	for( int i=0; i<gif_file->SHeight; ++i )
		memcpy(buffer[i], bg, row_size);
	int ret = read_next_image(output) > 0 ? 0 : 1;
	return ret;
}

// ret = -1:err, 0:eof, 1:frame
int FileGIF::read_next_image(VFrame *output)
{
	int ret = 0;
	GifRecordType record_type;

	while( !ret && !eof ) {
		if( DGifGetRecordType(gif_file, &record_type) == GIF_ERROR ) {
			err = gif_file->Error;
			eprintf("FileGIF::read_frame %d: %s\n", __LINE__, GifErrorString(err));
			ret = -1;
			break;
		}

		switch( record_type ) {
		case IMAGE_DESC_RECORD_TYPE: {
			if( DGifGetImageDesc(gif_file) == GIF_ERROR ) {
				err = gif_file->Error;
				eprintf("FileGIF::read_frame %d: %s\n", __LINE__, GifErrorString(err));
				break;
			}
			int row = gif_file->Image.Top;
			int col = gif_file->Image.Left;
			int width = gif_file->Image.Width;
			int height = gif_file->Image.Height;
			if( gif_file->Image.Left + gif_file->Image.Width > gif_file->SWidth ||
	 		    gif_file->Image.Top + gif_file->Image.Height > gif_file->SHeight )
				ret = -1;
			if( !ret && gif_file->Image.Interlace ) {
				static int InterlacedOffset[] = { 0, 4, 2, 1 };
				static int InterlacedJumps[] = { 8, 8, 4, 2 };
/* Need to perform 4 passes on the images: */
				for( int i=0; i<4; ++i ) {
					int j = row + InterlacedOffset[i];
					for( ; !ret && j<row + height; j+=InterlacedJumps[i] ) {
						if( DGifGetLine(gif_file, &buffer[j][col], width) == GIF_ERROR )
							ret = -1;
					}
				}
			}
			else {
				for( int i=0; !ret && i<height; ++i ) {
					if (DGifGetLine(gif_file, &buffer[row++][col], width) == GIF_ERROR)
						ret = -1;
				}
			}
			ret = 1;
			break; }
		case EXTENSION_RECORD_TYPE: {
			int ExtFunction = 0;
			GifByteType *ExtData = 0;
			if( DGifGetExtension(gif_file, &ExtFunction, &ExtData) == GIF_ERROR )
				ret = -1;
			while( !ret && ExtData ) {
				if( DGifGetExtensionNext(gif_file, &ExtData) == GIF_ERROR )
					ret = -1;
			}
			break; }
		case TERMINATE_RECORD_TYPE:
			eof = 1;
			break;
		default:
			ret = -1;
			break;
		}
	}

	ColorMapObject *color_map = 0;
	if( ret > 0 ) {
		color_map = gif_file->Image.ColorMap;
		if( !color_map ) color_map = gif_file->SColorMap;
		if( !color_map ) ret = -1;
	}
	if( ret > 0 && output ) {
		int screen_width = gif_file->SWidth;
		int screen_height = gif_file->SHeight;
		for( int i=0; i<screen_height; ++i ) {
			GifRowType row = buffer[i];
			unsigned char *out_ptr = output->get_rows()[i];
			for( int j=0; j<screen_width; ++j ) {
				GifColorType *color_map_entry = &color_map->Colors[row[j]];
				*out_ptr++ = color_map_entry->Red;
				*out_ptr++ = color_map_entry->Green;
				*out_ptr++ = color_map_entry->Blue;
			}
		}
	}
	return ret;
}

int FileGIF::write_frame(VFrame *frame)
{
	int w = frame->get_w(), h = frame->get_h();
	ColorMapObject *cmap = 0;
	int cmap_sz = depth >= 0 ? 1 << depth : 0;
	int64_t len = w * h * sizeof(GifByteType);
	GifByteType *bfr = (GifByteType *) malloc(len);
	int result = !bfr ? 1 : 0;
	if( !result ) {
		VFrame gbrp(w, h, BC_GBRP);
		gbrp.transfer_from(frame);
		if( !(cmap = GifMakeMapObject(cmap_sz, 0)) )
			result = 1;
		if( !result ) {
			GifByteType *gp = (GifByteType *)gbrp.get_r();
			GifByteType *bp = (GifByteType *)gbrp.get_g();
			GifByteType *rp = (GifByteType *)gbrp.get_b();
			if( GifQuantizeBuffer(w, h, &cmap_sz, rp, gp, bp,
					bfr, cmap->Colors) == GIF_ERROR )
				result = 1;
		}
	}
	if( !result && !writes &&
	    EGifPutScreenDesc(gif_file, w, h, depth, 0, 0) == GIF_ERROR )
		result = 1;
	if( !result &&
	    EGifPutImageDesc(gif_file, 0, 0, w, h, 0, cmap) == GIF_ERROR )
		result = 1;

	GifByteType *bp = bfr;
	for( int y=0; !result && y<h; ++y ) {
	        if( EGifPutLine(gif_file, bp, w) == GIF_ERROR )
			result = 1;
		bp += w;
	}
	GifFreeMapObject(cmap);
	if( bfr ) free(bfr);
	++writes;
	return result;
}

static int write_data(GifFileType *gif_file, const GifByteType *bfr, int bytes)
{
	FileGIF *file = (FileGIF*)gif_file->UserData;
	VFrame *output = file->output;
	long size = output->get_compressed_size();
	long alloc = output->get_compressed_allocated();
	long len = size + bytes;
	if( len > alloc )
		output->allocate_compressed_data(2*size + bytes);
	unsigned char *data = output->get_data() + size;
	memcpy(data, bfr, bytes);
	output->set_compressed_size(len);
	return bytes;
}

int FileGIF::wopen_data(VFrame *output)
{
	int result = 0;
	gif_file = EGifOpen(this, write_data, &err);
	if( !gif_file ) {
		eprintf("FileGIF::wopen_data %d: %s\n", __LINE__, GifErrorString(err));
		result = 1;
	}
	if( !result ) {
		output->set_compressed_size(0);
		this->output = output;
		writes = 0;
	}
	return result;
}


FileGIFList::FileGIFList(Asset *asset, File *file)
 : FileList(asset, file, "GIFLIST", ".gif", FILE_UNKNOWN, FILE_GIF_LIST)
{
}

FileGIFList::~FileGIFList()
{
}

int FileGIFList::check_sig(Asset *asset)
{
	FILE *stream = fopen(asset->path, "rb");
	if( stream ) {
		unsigned char test[16];
		int ret = fread(test, 16, 1, stream);
		fclose(stream);
		if( ret < 1 ) return 1;
		if( test[0] == 'G' && test[1] == 'I' && test[2] == 'F' &&
		    test[3] == 'L' && test[4] == 'I' && test[5] == 'S' && test[6] == 'T')
			return 1;
	}
	return 0;
}

int FileGIFList::colormodel_supported(int colormodel) { return BC_RGB888; }
int FileGIFList::get_best_colormodel(Asset *asset, int driver) { return BC_RGB888; }

int FileGIFList::read_frame_header(char *path)
{
	FILE *stream = fopen(path, "rb");
	if( stream ) {
		unsigned char test[16];
		int ret = fread(test, 16, 1, stream);
		fclose(stream);
		if( ret < 1 ) return 1;
		asset->format = FILE_GIF_LIST;
		asset->width = test[6] | (test[7] << 8);
		asset->height = test[8] | (test[9] << 8);
		return 0;
	}
	perror(path);
	return 1;
}

int FileGIFList::read_frame(VFrame *output, char *path)
{
	Asset *asset = new Asset(path);
	FileGIF gif(asset, file);
	int ret = gif.ropen_path(path);
	if( !ret )
		ret = gif.read_frame(output);
	asset->remove_user();
	return ret;
}

int FileGIFList::write_frame(VFrame *frame, VFrame *data, FrameWriterUnit *unit)
{
	int native_cmodel = BC_RGB888;
	if( frame->get_color_model() != native_cmodel ) {
		GIFUnit *gif_unit = (GIFUnit *)unit;
		if( !gif_unit->temp_frame ) gif_unit->temp_frame =
			new VFrame(frame->get_w(), frame->get_h(), native_cmodel);
		gif_unit->temp_frame->transfer_from(frame);
		frame = gif_unit->temp_frame;
	}

	FileGIF gif(asset, file);
	int ret = gif.wopen_data(data);
	if( !ret )
		ret = gif.write_frame(frame);
	return ret;
}

FrameWriterUnit* FileGIFList::new_writer_unit(FrameWriter *writer)
{
	return new GIFUnit(this, writer);
}

int FileGIFList::verify_file_list()
{
 // go through all .gif files in the list and
 // verify their sizes match or not.
 //printf("\nAsset Path: %s\n", asset->path);
 FILE *stream = fopen(asset->path, "rb");
 if (stream) {
  char string[BCTEXTLEN];
  int width, height, prev_width=-1, prev_height=-1;
  // build the path prefix
  char prefix[BCTEXTLEN], *bp = prefix, *cp = strrchr(asset->path, '/');
  for( int i=0, n=!cp ? 0 : cp-asset->path; i<n; ++i ) *bp++ = asset->path[i];
  *bp = 0;
  // read entire input file
  while( !feof(stream) && fgets(string, BCTEXTLEN, stream) ) {
   int len = strlen(string);
   if(!len || string[0] == '#' || string[0] == ' ' || isalnum(string[0])) continue;
   if( string[len-1] == '\n' ) string[len-1] = 0;
   // a possible .gif file path? fetch it
   char path[BCTEXTLEN], *pp = path, *ep = pp + sizeof(path)-1;
   if( string[0] == '.' && string[1] == '/' && prefix[0] )
    pp += snprintf(pp, ep-pp, "%s/", prefix);
   snprintf(pp, ep-pp, "%s", string);
   // check if a valid file exists
   if(!access(path, R_OK)) {
    // check file header for size
    FILE *gif_file_temp = fopen(path, "rb");
    if (gif_file_temp) {
     unsigned char test[16];
     int ret = fread(test, 16, 1, gif_file_temp);
     fclose(gif_file_temp);
     if( ret < 1 ) continue;
     // get height and width of gif file
     width = test[6] | (test[7] << 8);
     height = test[8] | (test[9] << 8);
     // test with previous
     if ( (prev_width == -1) && (prev_height == -1) ) {
      prev_width = width;
      prev_height = height;
      continue;
     }
     else if ( (prev_width != width) || (prev_height != height) ) {
      // this is the error case we are trying to avoid
      fclose(stream);
      return 0;
     }
    }

   }
  }
  fclose(stream);
  return 1;
 }
 // not sure if our function should be the one to raise not found error
 perror(asset->path);
 return 0;
}


GIFUnit::GIFUnit(FileGIFList *file, FrameWriter *writer)
 : FrameWriterUnit(writer)
{
	this->file = file;
	temp_frame = 0;
}

GIFUnit::~GIFUnit()
{
	delete temp_frame;
}

#endif
