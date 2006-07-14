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
 * No distribution of modified PDCurses code may be made under the name	*
 * "PDCurses", except by the current maintainer. (Although PDCurses is	*
 * public domain, the name is a trademark.)				*
 *									*
 * See the file maintain.er for details of the current maintainer.	*
 ************************************************************************/

#define CURSES_LIBRARY 1
#include <curses.h>

RCSID("$Id: pdckey.c,v 1.3 2006/07/14 19:22:02 wmcbrine Exp $");

/*man-start**************************************************************

  PDC_breakout()	- check for type-ahead

  PDCurses Description:
	Check if input is pending, either directly from the keyboard,
	or previously buffered.

  PDCurses Return Value:
	The PDC_breakout() routine returns TRUE if keyboard input is 
	pending otherwise FALSE is returned.

  Portability:
	PDCurses  bool PDC_breakout(void);

**man-end****************************************************************/

bool PDC_breakout(void)
{
	bool rc;

	PDC_LOG(("PDC_breakout() - called\n"));

	/* ungotten or buffered char */
	rc = (c_ungind) || (c_pindex > c_gindex);

	if (!rc)
		rc = PDC_check_bios_key();

	PDC_LOG(("PDC_breakout() - rc %d c_ungind %d c_pindex %d c_gindex %d\n",
		 rc, c_ungind, c_pindex, c_gindex));

	return rc;
}

/*man-start**************************************************************

  PDC_rawgetch()  - Returns the next uninterpreted character (if available).

  PDCurses Description:
	Gets a character without any interpretation at all and returns 
	it. If keypad mode is active for the designated window, function 
	key translation will be performed.  Otherwise, function keys are 
	ignored.  If nodelay mode is active in the window, then 
	PDC_rawgetch() returns -1 if no character is available.

	WARNING:  It is unknown whether the FUNCTION key translation
	is performed at this level. --Frotz 911130 BUG

  PDCurses Return Value:
	This function returns OK on success and ERR on error.

  PDCurses Errors:
	No errors are defined for this function.

  Portability:
	PDCurses  int PDC_rawgetch(void);

**man-end****************************************************************/

int PDC_rawgetch(void)
{
	int c, oldc;

	PDC_LOG(("PDC_rawgetch() - called\n"));

	if (_getch_win_ == (WINDOW *)NULL)
		return -1;

	if ((SP->delaytenths || _getch_win_->_delayms || _getch_win_->_nodelay)
	    && !PDC_breakout())
		return -1;

	for (;;)
	{
		c = PDC_get_bios_key();
		oldc = c;

		/* return the key if it is not a special key */

		if (c != KEY_MOUSE && (c = PDC_validchar(c)) >= 0)
			return c;

		if (_getch_win_->_use_keypad)
			return oldc;
	}
}

/*man-start**************************************************************

  PDC_sysgetch()  - Return a character using default system routines.

  PDCurses Description:
	This is a private PDCurses function.

	Gets a character without normal ^S, ^Q, ^P and ^C interpretation
	and returns it.  If keypad mode is active for the designated
	window, function key translation will be performed. Otherwise,
	function keys are ignored. If nodelay mode is active in the
	window, then sysgetch() returns -1 if no character is
	available.

  PDCurses Return Value:
	This function returns OK upon success otherwise ERR is returned.

  PDCurses Errors:
	No errors are defined for this routine.

  Portability:
	PDCurses  int PDC_sysgetch(void);

**man-end****************************************************************/

int PDC_sysgetch(void)
{
	int c;

	PDC_LOG(("PDC_sysgetch() - called\n"));

	if (_getch_win_ == (WINDOW *)NULL)
		return -1;

	if ((SP->delaytenths || _getch_win_->_delayms || _getch_win_->_nodelay)
	    && !PDC_breakout())
		return -1;

	for (;;)
	{
		c = PDC_get_bios_key();

		/* return the key if it is not a special key */

		if ((unsigned int)c < 256)
			return c;

		if ((c = PDC_validchar(c)) >= 0)
			return c;
	}
}
