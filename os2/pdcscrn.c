/************************************************************************ 
 * This file is part of PDCurses. PDCurses is public domain software;	*
 * you may use it for any purpose. This software is provided AS IS with	*
 * NO WARRANTY whatsoever.						*
 *									*
 * If you use PDCurses in an application, an acknowledgement would be	*
 * appreciated, but is not mandatory. If you make corrections or	*
 * enhancements to PDCurses, please forward them to the current		*
 * maintainer for the benefit of other users.				*
 *									*
 * See the file maintain.er for details of the current maintainer.	*
 ************************************************************************/

#include "pdcos2.h"

RCSID("$Id: pdcscrn.c,v 1.62 2006/12/24 04:32:22 wmcbrine Exp $");

int pdc_font;			/* default font size	*/

#ifdef EMXVIDEO
static unsigned char *saved_screen = NULL;
static int saved_lines = 0;
static int saved_cols = 0;
#else

# ifdef __WATCOMC__
#  define PDCTHUNK(x) ((ptr_16)(x))
# else
#  ifdef __EMX__
#   define PDCTHUNK(x) ((PCH)_emx_32to16(x))
#  endif
# endif

# if defined(__WATCOMC__) && defined(__386__)
#  define SEG16 _Seg16
# else
#  define SEG16
# endif

# ifdef PDCTHUNK

#  ifdef __EMX__
#   define THUNKEDVIO VIOCOLORREG
#  else

typedef void * SEG16 ptr_16;
typedef struct {
	USHORT cb;
	USHORT type;
	USHORT firstcolorreg;
	USHORT numcolorregs;
	ptr_16 colorregaddr;
} THUNKEDVIO;

#  endif
# endif

static PCH saved_screen = NULL;
static USHORT saved_lines = 0;
static USHORT saved_cols = 0;
static VIOMODEINFO scrnmode;	/* default screen mode	*/
static VIOMODEINFO saved_scrnmode[3];
static int saved_font[3];
static bool can_change = FALSE;

extern void PDC_get_keyboard_info(void);
extern void PDC_set_keyboard_default(void);

static int _get_font(void)
{
	VIOMODEINFO modeInfo = {0};

	modeInfo.cb = sizeof(modeInfo);

	VioGetMode(&modeInfo, 0);
	return (modeInfo.vres / modeInfo.row);
}

static void _set_font(int size)
{
	VIOMODEINFO modeInfo = {0};

	if (pdc_font != size)
	{
		modeInfo.cb = sizeof(modeInfo);

		/* set most parameters of modeInfo */

		VioGetMode(&modeInfo, 0);
		modeInfo.cb = 8;	/* ignore horiz an vert resolution */
		modeInfo.row = modeInfo.vres / size;
		VioSetMode(&modeInfo, 0);
	}

	curs_set(SP->visibility);

	pdc_font = _get_font();
}

#endif

/*man-start**************************************************************

  PDC_scr_close() - Internal low-level binding to close the physical screen

  PDCurses Description:
	May restore the screen to its state before PDC_scr_open();
	miscellaneous cleanup.

  PDCurses Return Value:
	This function returns OK on success, otherwise an ERR is returned.

  Portability:
	PDCurses  void PDC_scr_close(void);

**man-end****************************************************************/

void PDC_scr_close(void)
{
	PDC_LOG(("PDC_scr_close() - called\n"));

	if (saved_screen && getenv("PDC_RESTORE_SCREEN"))
	{
#ifdef EMXVIDEO
		v_putline(saved_screen, 0, 0, saved_lines * saved_cols);
#else
		VioWrtCellStr(saved_screen, saved_lines * saved_cols * 2,
			0, 0, (HVIO)NULL);
#endif
		free(saved_screen);
		saved_screen = NULL;
	}

	reset_shell_mode();

	if (SP->visibility != 1)
		curs_set(1);

	/* Position cursor to the bottom left of the screen. */

	PDC_gotoyx(PDC_get_rows() - 2, 0);
}

void PDC_scr_free(void)
{
	if (SP)
		free(SP);
	if (pdc_atrtab)
		free(pdc_atrtab);
}

/*man-start**************************************************************

  PDC_scr_open()  - Internal low-level binding to open the physical screen

  PDCurses Description:
	The platform-specific part of initscr() -- allocates SP, does
	miscellaneous intialization, and may save the existing screen
	for later restoration.

  PDCurses Return Value:
	This function returns OK on success, otherwise an ERR is returned.

  Portability:
	PDCurses  int PDC_scr_open(int argc, char **argv);

**man-end****************************************************************/

int PDC_scr_open(int argc, char **argv)
{
#ifdef EMXVIDEO
	int adapter;
#else
	USHORT totchars;
#endif
	short r, g, b;

	PDC_LOG(("PDC_scr_open() - called\n"));

	SP = calloc(1, sizeof(SCREEN));
	pdc_atrtab = calloc(MAX_ATRTAB, 1);

	if (!SP || !pdc_atrtab)
		return ERR;

#ifdef EMXVIDEO
        v_init();
#endif
	SP->orig_attr = FALSE;

	PDC_get_cursor_pos(&SP->cursrow, &SP->curscol);

#ifdef EMXVIDEO
	adapter = v_hardware();
	SP->mono = (adapter == V_MONOCHROME);

	pdc_font = SP->mono ? 14 : (adapter == V_COLOR_8) ? 8 : 12;
#else
	VioGetMode(&scrnmode, 0);
	PDC_get_keyboard_info();

	/* Now set the keyboard into binary mode */

	PDC_set_keyboard_binary(TRUE);

	pdc_font = _get_font();
#endif
	SP->lines = PDC_get_rows();
	SP->cols = PDC_get_columns();

	SP->mouse_wait = 100;

	/* This code for preserving the current screen */

	if (getenv("PDC_RESTORE_SCREEN"))
	{
		saved_lines = SP->lines;
		saved_cols = SP->cols;

		saved_screen = malloc(2 * saved_lines * saved_cols);

		if (!saved_screen)
		{
			SP->_preserve = FALSE;
			return OK;
		}
#ifdef EMXVIDEO
		v_getline(saved_screen, 0, 0, saved_lines * saved_cols);
#else
		totchars = saved_lines * saved_cols * 2;
		VioReadCellStr((PCH)saved_screen, &totchars, 0, 0, (HVIO)NULL);
#endif
	}

	SP->_preserve = (getenv("PDC_PRESERVE_SCREEN") != NULL);

	can_change = (PDC_color_content(0, &r, &g, &b) == OK);

	return OK;
}

/*man-start**************************************************************

  PDC_resize_screen()   - Internal low-level function to resize screen

  PDCurses Description:
	This function provides a means for the application program to
	resize the overall dimensions of the screen.  Under DOS and OS/2
	the application can tell PDCurses what size to make the screen;
	under X11, resizing is done by the user and this function simply
	adjusts its internal structures to fit the new size.

  PDCurses Return Value:
	This function returns OK on success, otherwise an ERR is 
	returned.

  PDCurses Errors:

  Portability:
	PDCurses  int PDC_resize_screen(int, int);

**man-end****************************************************************/

int PDC_resize_screen(int nlines, int ncols)
{
#ifndef EMXVIDEO
	VIOMODEINFO modeInfo = {0};
	USHORT result;
#endif

	PDC_LOG(("PDC_resize_screen() - called. Lines: %d Cols: %d\n",
		nlines, ncols));

#ifdef EMXVIDEO
	return ERR;
#else
	modeInfo.cb = sizeof(modeInfo);

	/* set most parameters of modeInfo */

	VioGetMode(&modeInfo, 0);
	modeInfo.fbType = 1;
	modeInfo.row = nlines;
	modeInfo.col = ncols;
	result = VioSetMode(&modeInfo, 0);

	LINES = PDC_get_rows();
	COLS = PDC_get_columns();

	return (result == 0) ? OK : ERR;
#endif
}

void PDC_reset_prog_mode(void)
{
	PDC_LOG(("PDC_reset_prog_mode() - called.\n"));

#ifndef EMXVIDEO
	PDC_set_keyboard_binary(TRUE);
#endif
}

void PDC_reset_shell_mode(void)
{
	PDC_LOG(("PDC_reset_shell_mode() - called.\n"));

#ifndef EMXVIDEO
	PDC_set_keyboard_default();
#endif
}

#ifndef EMXVIDEO

static bool _screen_mode_equals(VIOMODEINFO *oldmode)
{
	VIOMODEINFO current = {0};

	VioGetMode(&current, 0);

	return ((current.cb == oldmode->cb) &&
		(current.fbType == oldmode->fbType) &&
		(current.color == oldmode->color) && 
		(current.col == oldmode->col) &&
		(current.row == oldmode->row) && 
		(current.hres == oldmode->vres) &&
		(current.vres == oldmode->vres));
}

#endif

void PDC_restore_screen_mode(int i)
{
#ifndef EMXVIDEO
	if (i >= 0 && i <= 2)
	{
		pdc_font = _get_font();
		_set_font(saved_font[i]);

		if (!_screen_mode_equals(&saved_scrnmode[i]))
			if (VioSetMode(&saved_scrnmode[i], 0) != 0)
			{
				pdc_font = _get_font();
				scrnmode = saved_scrnmode[i];
				LINES = PDC_get_rows();
				COLS = PDC_get_columns();
			}
	}
#endif
}

void PDC_save_screen_mode(int i)
{
#ifndef EMXVIDEO
	if (i >= 0 && i <= 2)
	{
		saved_font[i] = pdc_font;
		saved_scrnmode[i] = scrnmode;
	}
#endif
}

bool PDC_can_change_color(void)
{
	return can_change;
}

int PDC_color_content(short color, short *red, short *green, short *blue)
{
#ifdef PDCTHUNK
	THUNKEDVIO vcr;
	USHORT palbuf[4];
	unsigned char pal[3];
	int rc;

	/* Read single DAC register */

	palbuf[0] = 8;
	palbuf[1] = 0;
	palbuf[2] = color;

	rc = VioGetState(&palbuf, 0);
	if (rc)
		return ERR;

	vcr.cb = sizeof(vcr);
	vcr.type = 3;
	vcr.firstcolorreg = palbuf[3];
	vcr.numcolorregs = 1;
	vcr.colorregaddr = PDCTHUNK(pal);

	rc = VioGetState(&vcr, 0);
	if (rc)
		return ERR;

	/* Scale and store */

	*red = DIVROUND((unsigned)(pal[0]) * 1000, 63);
	*green = DIVROUND((unsigned)(pal[1]) * 1000, 63);
	*blue = DIVROUND((unsigned)(pal[2]) * 1000, 63);

	return OK;
#else
	return ERR;
#endif
}

int PDC_init_color(short color, short red, short green, short blue)
{
#ifdef PDCTHUNK
	THUNKEDVIO vcr;
	USHORT palbuf[4];
	unsigned char pal[3];
	int rc;

	/* Scale */

	pal[0] = DIVROUND((unsigned)red * 63, 1000);
	pal[1] = DIVROUND((unsigned)green * 63, 1000);
	pal[2] = DIVROUND((unsigned)blue * 63, 1000);

	/* Set single DAC register */

	palbuf[0] = 8;
	palbuf[1] = 0;
	palbuf[2] = color;

	rc = VioGetState(&palbuf, 0);
	if (rc)
		return ERR;

	vcr.cb = sizeof(vcr);
	vcr.type = 3;
	vcr.firstcolorreg = palbuf[3];
	vcr.numcolorregs = 1;
	vcr.colorregaddr = PDCTHUNK(pal);

	rc = VioSetState(&vcr, 0);

	return rc ? ERR : OK;
#else
	return ERR;
#endif
}
