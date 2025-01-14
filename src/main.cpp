// Matrix Screensaver
// Copyright (c) J Brown 2003 (catch22.net)

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#include "main.h"
#include "resource.h"

//
// this isn't really a random-number generator. It's based on
// a 16bit CRC algorithm. With the right mask (0xb400) it is possible
// to call this function 65536 times and get a unique result every time
// with *NO* repeats. The results look random but they're not - if we
// call this function another 65536 times we get exactly the same results
// in the same order. This is necessary for fading in messages because
// we need to be guaranteed that all cells...it's completely uniform in
// operation but looks random enough to be very effective
//
WORD crc_msgrand (WORD reg)
{
	const WORD mask = 0xb400;

	if (reg & 1)
		reg = (reg >> 1) ^ mask;
	else
		reg = (reg >> 1);

	return reg;
}

int crc_rand ()
{
	const  WORD mask = 0xb400;

	if (_crc_reg & 1)
		_crc_reg = (_crc_reg >> 1) ^ mask;
	else
		_crc_reg = (_crc_reg >> 1);

	return _crc_reg;
}

static double HuetoRGB (double m1, double m2, double h)
{
	if (h < 0) h += 1.0;
	if (h > 1) h -= 1.0;

	if (6.0*h < 1)
		return (m1 + (m2 - m1)*h*6.0);

	if (2.0*h < 1)
		return m2;

	if (3.0*h < 2.0)
		return (m1 + (m2 - m1)*((2.0 / 3.0) - h)*6.0);

	return m1;
}

COLORREF HSLtoRGB (double H, double S, double L)
{
	double r, g, b;
	double m1, m2;

	if (S == 0)
	{
		r = g = b = L;
	}
	else
	{
		if (L <= 0.5)
			m2 = L*(1.0 + S);
		else
			m2 = L + S - L*S;
		m1 = 2.0*L - m2;

		r = HuetoRGB (m1, m2, H + 1.0 / 3.0);
		g = HuetoRGB (m1, m2, H);
		b = HuetoRGB (m1, m2, H - 1.0 / 3.0);

	}

	return RGB ((BYTE)(r * 255), (BYTE)(g * 255), (BYTE)(b * 255));
}

COLORREF RGBtoHSL (COLORREF col, double *H, double *S, double *L)
{
	double r = GetRValue (col) / 255.0;
	double g = GetGValue (col) / 255.0;
	double b = GetBValue (col) / 255.0;

	double cMin = min (min (r, g), b);
	double cMax = max (max (r, g), b);
	double l = (cMax + cMin) / 2.0;
	double h, s;
	double delta = cMax - cMin;

	if (delta == 0)
	{
		s = 0;
		h = 0;
	}
	else
	{
		if (l <= 0.5)
			s = (cMax - cMin) / (cMax + cMin);
		else
			s = (cMax - cMin) / (2.0 - cMax - cMin);

		if (r == cMax)
			h = (g - b) / delta;
		else if (g == cMax)
			h = 2.0 + (b - r) / delta;
		else
			h = 4.0 + (r - g) / delta;

		h /= 6.0;
		if (h < 0)
			h += 1.0;

	}

	*H = h;
	*S = s;
	*L = l;
	return 0;
}

MATRIX *GetMatrix (HWND hwnd)
{
	return (MATRIX*)GetWindowLong (hwnd, 0);
}

void SetMatrix (HWND hwnd, MATRIX* matrix)
{
	SetWindowLong (hwnd, 0, (LONG)matrix);
}

int GlyphIntensity (GLYPH glyph)
{
	return (int)((glyph & 0x7f00) >> 8);
}

GLYPH RandomGlyph (int intensity)
{
	return GLYPH_REDRAW | (intensity << 8) | (crc_rand () % NUM_GLYPHS);
}

GLYPH DarkenGlyph (GLYPH glyph)
{
	int intensity = GlyphIntensity (glyph);

	if (intensity > 0)
		return GLYPH_REDRAW | ((intensity - 1) << 8) | (glyph & 0x00FF);
	else
		return glyph;
}

void DrawGlyph (MATRIX *matrix, HDC hdc, int xpos, int ypos, GLYPH glyph)
{
	int intensity = GlyphIntensity (glyph);
	int glyphidx = glyph & 0xff;

	BitBlt (hdc, xpos, ypos, GLYPH_WIDTH, GLYPH_HEIGHT, matrix->hdcBitmap,
		glyphidx * GLYPH_WIDTH, intensity * GLYPH_HEIGHT, SRCCOPY);
}

void RedrawBlip (GLYPH *glypharr, int blippos)
{
	glypharr[blippos + 0] |= GLYPH_REDRAW;
	glypharr[blippos + 1] |= GLYPH_REDRAW;
	glypharr[blippos + 8] |= GLYPH_REDRAW;
	glypharr[blippos + 9] |= GLYPH_REDRAW;
}

void ScrollMatrixColumn (MATRIX_COLUMN* col)
{
	int y = 0;
	GLYPH lastglyph = 0;
	GLYPH thisglyph = 0;

	// wait until we are allowed to scroll
	if (!col->started)
	{
		if (--col->countdown <= 0)
			col->started = TRUE;

		return;
	}

	// "seed" the glyph-run
	lastglyph = col->state ? (GLYPH)0 : (GLYPH)(MAX_INTENSITY << 8);

	//
	// loop over the entire length of the column, looking for changes
	// in intensity/darkness. This change signifies the start/end
	// of a run of glyphs.
	//
	for (y = 0; y < col->length; y++)
	{
		thisglyph = col->glyph[y];

		// bottom-most part of "run". Insert a new character (glyph)
		// at the end to lengthen the run down the screen..gives the
		// impression that the run is "falling" down the screen
		if (GlyphIntensity (thisglyph) < GlyphIntensity (lastglyph) &&
			GlyphIntensity (thisglyph) == 0)
		{
			col->glyph[y] = RandomGlyph (MAX_INTENSITY - 1);
			y++;
		}
		// top-most part of "run". Delete a character off the top by
		// darkening the glyph until it eventually disappears (turns black). 
		// this gives the effect that the run as dropped downwards
		else if (GlyphIntensity (thisglyph) > GlyphIntensity (lastglyph))
		{
			col->glyph[y] = DarkenGlyph (thisglyph);

			// if we've just darkened the last bit, skip on so
			// the whole run doesn't go dark
			if (GlyphIntensity (thisglyph) == MAX_INTENSITY - 1)
				y++;
		}

		lastglyph = col->glyph[y];
	}

	// change state from blanks <-> runs when the current run as expired
	if (--col->runlen <= 0)
	{
		int density = DENSITY;
		density = DENSITY_MAX - density + DENSITY_MIN;
		if (col->state ^= 1)
			col->runlen = crc_rand () % (3 * density + DENSITY_MIN);
		else
			col->runlen = crc_rand () % (DENSITY_MAX - density + 1) + (DENSITY_MIN * 2);
	}

	//
	// make a "blip" run down this column at double-speed
	//

	// mark current blip as redraw so it gets "erased"
	if (col->blippos >= 0 && col->blippos < col->length)
		RedrawBlip (col->glyph, col->blippos);

	// advance down screen at double-speed
	col->blippos += 2;

	// if the blip gets to the end of a run, start it again (for a random
	// length so that the blips never get synched together)
	if (col->blippos >= col->bliplen)
	{
		col->bliplen = col->length + crc_rand () % 50;
		col->blippos = 0;
	}

	// now redraw blip at new position
	if (col->blippos >= 0 && col->blippos < col->length)
		RedrawBlip (col->glyph, col->blippos);

}

//
// randomly change a small collection glyphs in a column
//
void RandomMatrixColumn (MATRIX_COLUMN *col)
{
	int i, y;

	for (i = 1, y = 0; i < 16; i++)
	{
		// find a run
		while (GlyphIntensity (col->glyph[y]) < MAX_INTENSITY - 1 && y < col->length)
			y++;

		if (y >= col->length)
			break;

		col->glyph[y] = (col->glyph[y] & 0xff00) | (crc_rand () % NUM_GLYPHS);
		col->glyph[y] |= GLYPH_REDRAW;

		y += crc_rand () % 10;
	}
}

void RedrawMatrixColumn (MATRIX_COLUMN *col, MATRIX *matrix, HDC hdc, int xpos)
{
	int y;

	// loop down the length of the column redrawing only what needs doing
	for (y = 0; y < col->length; y++)
	{
		GLYPH glyph = col->glyph[y];

		// does this glyph (character) need to be redrawn?
		if (glyph & GLYPH_REDRAW)
		{
			if ((y == col->blippos + 0 || y == col->blippos + 1 ||
				y == col->blippos + 8 || y == col->blippos + 9) &&
				GlyphIntensity (glyph) >= MAX_INTENSITY - 1)
				glyph |= MAX_INTENSITY << 8;

			DrawGlyph (matrix, hdc, xpos, y * GLYPH_HEIGHT, glyph);

			// clear redraw state
			col->glyph[y] &= ~GLYPH_REDRAW;
		}
	}
}

HBITMAP MakeBitmap (HINSTANCE h, UINT uId, double hue)
{
	HBITMAP hDIB;
	HANDLE  hOldBmp;
	HBITMAP hBmp;
	HDC		hdc;

	DIBSECTION dib;
	BITMAPINFOHEADER *bih;
	DWORD *dest = 0;
	BYTE  *src;
	int i;

	RGBQUAD pal[256];

	// load the 8bit image 
	hBmp = (HBITMAP)LoadImage (h, MAKEINTRESOURCE (IDB_GLYPH), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);

	// extract the colour table
	hdc = CreateCompatibleDC (nullptr);
	hOldBmp = SelectObject (hdc, hBmp);
	GetDIBColorTable (hdc, 0, 256, pal);
	SelectObject (hdc, hOldBmp);

	// 
	GetObject (hBmp, sizeof (dib), &dib);
	src = (BYTE*)dib.dsBm.bmBits;
	bih = &dib.dsBmih;

	// change to a 32bit bitmap
	dib.dsBm.bmBitsPixel = 32;
	dib.dsBm.bmPlanes = 1;
	dib.dsBm.bmWidthBytes = dib.dsBm.bmWidth * 4;
	dib.dsBm.bmType = 0;
	dib.dsBmih.biBitCount = 32;
	dib.dsBmih.biPlanes = 1;
	dib.dsBmih.biCompression = 0;
	dib.dsBmih.biSizeImage = dib.dsBmih.biWidth * dib.dsBmih.biHeight * 4;
	dib.dsBmih.biClrUsed = 0;
	dib.dsBmih.biClrImportant = 0;

	// create a new (blank) 32bit DIB section
	hDIB = CreateDIBSection (hdc, (BITMAPINFO *)&dib.dsBmih, DIB_RGB_COLORS, (void**)&dest, 0, 0);

	// copy each pixel
	for (i = 0; i < bih->biWidth * bih->biHeight; i++)
	{
		// convert 8bit palette entry to 32bit colour
		RGBQUAD  rgb = pal[*src++];
		COLORREF col = RGB (rgb.rgbRed, rgb.rgbGreen, rgb.rgbBlue);

		// convert the RGB colour to H,S,L values
		double h, s, l;
		RGBtoHSL (col, &h, &s, &l);

		// create the new colour 
		*dest++ = HSLtoRGB (hue, s, l);
	}

	DeleteObject (hBmp);
	DeleteDC (hdc);
	return hDIB;
}

void SetMatrixBitmap (MATRIX *matrix, INT hue)
{
	HBITMAP hBmp;

	hue %= 255;

	// create the new bitmap
	hBmp = MakeBitmap (GetModuleHandle (0), IDB_GLYPH, hue / 255.0);
	DeleteObject (SelectObject (matrix->hdcBitmap, hBmp));

	matrix->hbmBitmap = hBmp;
	SelectObject (matrix->hdcBitmap, matrix->hbmBitmap);
}

void DecodeMatrix (HWND hwnd, MATRIX *matrix)
{
	HDC hdc = GetDC (hwnd);

	for (int x = 0; x < matrix->numcols; x++)
	{
		RandomMatrixColumn (&matrix->column[x]);
		ScrollMatrixColumn (&matrix->column[x]);
		RedrawMatrixColumn (&matrix->column[x], matrix, hdc, x * GLYPH_WIDTH);
	}

	SetMatrixBitmap (matrix, HUE);

	ReleaseDC (hwnd, hdc);
}

void RefreshMatrix (HWND hwnd)
{
	int x, y;
	MATRIX *matrix = GetMatrix (hwnd);
	for (x = 0; x < matrix->numcols; x++)
	{
		for (y = 0; y < matrix->column[x].length; y++)
			matrix->column[x].glyph[y] |= GLYPH_REDRAW;
	}

	SendMessage (hwnd, WM_TIMER, 0, 0);
}

//
//	Allocate matrix structures
//
MATRIX *CreateMatrix (HWND hwnd, int width, int height)
{
	MATRIX *matrix;
	HDC hdc;
	int x, y;

	int rows = height / GLYPH_HEIGHT + 1;
	int cols = width / GLYPH_WIDTH + 1;

	// allocate matrix!
	if ((matrix = (MATRIX*)malloc (sizeof (MATRIX) + sizeof (MATRIX_COLUMN) * cols)) == 0)
		return 0;

	matrix->numcols = cols;
	matrix->numrows = rows;
	matrix->width = width;
	matrix->height = height;

	for (x = 0; x < cols; x++)
	{
		matrix->column[x].length = rows;
		matrix->column[x].started = FALSE;
		matrix->column[x].countdown = crc_rand () % 100;
		matrix->column[x].state = crc_rand () % 2;
		matrix->column[x].runlen = crc_rand () % 20 + 3;

		matrix->column[x].glyph = (GLYPH*)malloc (sizeof (GLYPH) * (rows + 16));

		for (y = 0; y < rows; y++)
			matrix->column[x].glyph[y] = 0;
	}

	// Load bitmap!!
	hdc = GetDC (nullptr);
	matrix->hbmBitmap = MakeBitmap (GetModuleHandle (nullptr), IDB_GLYPH, HUE / 255.0);
	matrix->hdcBitmap = CreateCompatibleDC (hdc);
	SelectObject (matrix->hdcBitmap, matrix->hbmBitmap);
	ReleaseDC (nullptr, hdc);

	return matrix;
}

//
//	Free up matrix structures
//
void DestroyMatrix (MATRIX *matrix)
{
	int x;

	// free the matrix columns
	for (x = 0; x < matrix->numcols; x++)
		free (matrix->column[x].glyph);

	DeleteDC (matrix->hdcBitmap);
	DeleteObject (matrix->hbmBitmap);

	// now delete the matrix!
	free (matrix);
}

LRESULT CALLBACK ScreensaverProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	MATRIX* matrix = GetMatrix (hwnd);

	static POINT ptLast = {0};
	static POINT ptCursor = {0};
	static BOOL fFirstTime = TRUE;

	switch (msg)
	{
		case WM_NCCREATE:
		{
			matrix = CreateMatrix (hwnd, LPCREATESTRUCT (lparam)->cx, LPCREATESTRUCT (lparam)->cy);

			if (matrix)
			{
				SetMatrix (hwnd, matrix);
				SetTimer (hwnd, 0xdeadbeef, ((SPEED_MAX - SPEED) + SPEED_MIN) * 10, 0);

				return TRUE;
			}

			return FALSE;
		}

		case WM_NCDESTROY:
		{
			DestroyMatrix (matrix);
			PostQuitMessage (0);

			return FALSE;
		}

		case WM_CLOSE:
		{
			KillTimer (hwnd, 0xdeadbeef);
			DestroyWindow (hwnd);

			return FALSE;
		}

		case WM_TIMER:
		{
			DecodeMatrix (hwnd, matrix);
			return FALSE;
		}

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		{
			DestroyWindow (hwnd);
			return FALSE;
		}

		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MOUSEMOVE:
		{
			if (fFirstTime)
			{
				GetCursorPos (&ptLast);
				fFirstTime = FALSE;
			}

			GetCursorPos (&ptCursor);

			if (abs (ptCursor.x - ptLast.x) >= 5 || abs (ptCursor.y - ptLast.y) >= 5)
			{
				DestroyWindow (hwnd);
			}

			ptLast = ptCursor;

			return FALSE;
		}
	}

	return DefWindowProc (hwnd, msg, wparam, lparam);
}

BOOL CALLBACK MonitorEnumProc (HMONITOR, HDC, LPRECT lprc, LPARAM)
{
	HWND h = CreateWindowEx (WS_EX_TOPMOST, APP_NAME_SHORT, APP_NAME, WS_POPUP | WS_VISIBLE, lprc->left, lprc->top, lprc->right - lprc->left, lprc->bottom - lprc->top, nullptr, nullptr, GetModuleHandle (nullptr), nullptr);

	SetWindowPos (h, 0, lprc->left, lprc->top, 0, 0, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE | SWP_SHOWWINDOW);

	return TRUE;
}

INT APIENTRY wWinMain (HINSTANCE h, HINSTANCE, LPWSTR cmdline, INT)
{
	MSG msg = {0};
	WNDCLASSEX wcex = {0};
	INITCOMMONCONTROLSEX icex = {0};

	if (FindWindowEx (NULL, NULL, APP_NAME_SHORT, APP_NAME))
	{
		return 0;
	}

	if (wcsnicmp (L"/c", cmdline, 2) == 0)
	{
		MessageBox (nullptr, L"No settings.", APP_NAME, MB_OK | MB_ICONEXCLAMATION);
	}
	else if (wcsnicmp (L"/s", cmdline, 2) == 0)
	{
		UINT state = 0;

		SystemParametersInfo (SPI_SETSCREENSAVERRUNNING, TRUE, &state, 0);

		icex.dwSize = sizeof (icex);
		icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;

		InitCommonControlsEx (&icex);

		wcex.cbSize = sizeof (WNDCLASSEX);
		wcex.hInstance = h;
		wcex.lpszClassName = APP_NAME_SHORT;
		wcex.lpfnWndProc = ScreensaverProc;
		wcex.hbrBackground = (HBRUSH)GetStockObject (BLACK_BRUSH);
		wcex.hCursor = LoadCursor (h, MAKEINTRESOURCE (IDC_CURSOR));
		wcex.cbWndExtra = sizeof (MATRIX*);

		RegisterClassEx (&wcex);

		_crc_reg = (WORD)GetTickCount ();

		EnumDisplayMonitors (nullptr, nullptr, MonitorEnumProc, 0);

		while (GetMessage (&msg, nullptr, 0, 0))
		{
			TranslateMessage (&msg);
			DispatchMessage (&msg);
		}

		SystemParametersInfo (SPI_SETSCREENSAVERRUNNING, FALSE, &state, 0);
	}

	return 0;
}
