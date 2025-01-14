// Matrix Screensaver
// Copyright (c) J Brown 2003 (catch22.net)

#ifndef __MAIN_H__
#define __MAIN_H__

#include <windows.h>
#include "resource.h"

// config
#define APP_NAME L"Matrix Screensaver"
#define APP_NAME_SHORT L"matrix"
#define APP_VERSION L"1.1"
#define APP_VERSION_RES 1,1
#define APP_COPYRIGHT L"(c) J Brown 2003 (catch22.net)"

WORD _crc_reg = 0;

typedef unsigned short GLYPH;

#define GLYPH_REDRAW 0x8000
#define GLYPH_BLANK 0x4000

#define DENSITY 28
#define DENSITY_MAX 50
#define DENSITY_MIN 5

// constants inferred from matrix.bmp
#define MAX_INTENSITY 4 // number of intensity levels
#define NUM_GLYPHS 32 // number of "glyphs" in each level
#define GLYPH_WIDTH 14 // width  of each glyph (pixels)
#define GLYPH_HEIGHT 14 // height of each glyph (pixels)

#define SPEED_MAX 10
#define SPEED_MIN 1
#define SPEED 2

#define HUE 100

//
//	The "matrix" is basically an array of these
//  column structures, positioned side-by-side
//
typedef struct
{
	BOOL	state;
	int		countdown;

	BOOL	started;
	int		runlen;

	int		blippos;
	int		bliplen;

	int		length;
	GLYPH	*glyph;

} MATRIX_COLUMN;

typedef struct
{
	int				width;
	int				height;
	int				numcols;
	int				numrows;

	// bitmap containing glyphs.
	HDC				hdcBitmap;
	HBITMAP			hbmBitmap;

	MATRIX_COLUMN	column[1];

} MATRIX;

#endif // __MAIN_H__
