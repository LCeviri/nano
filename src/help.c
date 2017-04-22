/**************************************************************************
 *   help.c  --  This file is part of GNU nano.                           *
 *                                                                        *
 *   Copyright (C) 2000-2011, 2013-2017 Free Software Foundation, Inc.    *
 *   Copyright (C) 2014, 2015, 2016 Benno Schulenberg                     *
 *                                                                        *
 *   GNU nano is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU General Public License as published    *
 *   by the Free Software Foundation, either version 3 of the License,    *
 *   or (at your option) any later version.                               *
 *                                                                        *
 *   GNU nano is distributed in the hope that it will be useful,          *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty          *
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *   See the GNU General Public License for more details.                 *
 *                                                                        *
 *   You should have received a copy of the GNU General Public License    *
 *   along with this program.  If not, see http://www.gnu.org/licenses/.  *
 *                                                                        *
 **************************************************************************/

#include "proto.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#ifndef DISABLE_HELP

static char *help_text = NULL;
	/* The text displayed in the help window. */

static char *end_of_intro = NULL;
	/* The point in the help text where the introductory paragraphs end
	 * and the shortcut descriptions begin. */

const char *beg_of_intro = NULL;
	/* The point in the help text where the introductory paragraphs
	 * begin. */

char *tempfilename = NULL;
	/* Name of the safe temporary file that we will use for wrapping
	 * and writing the help text. */

/* Writes the hard wrapped help text in the temp file and displays it. */
void display_the_help_text(bool redisplaying)
{
    int line_size;
    const char *ptr = beg_of_intro;
	/* The current line of the help text. */
    FILE *fp = fopen(tempfilename, "w+b");

    if (fp == NULL) {
	statusline(ALERT, _("Error writing temp file: %s"), strerror(errno));
	return;
    }

    /* Wrap and copy the rest of the help_text into the temporary file. */
    while (strlen(ptr) > 0) {
	line_size = help_line_len(ptr);
	fwrite(ptr, sizeof(char), line_size, fp);
	ptr += line_size;

	/* Hard wrap the lines in the help text. */
	if (*ptr != '\n')
	    fwrite("\n", sizeof(char), 1, fp);
	else
	    while (*ptr == '\n')
		fwrite(ptr++, sizeof(char), 1, fp);
    }
    fclose(fp);

    if (redisplaying)
	close_buffer();

    if (!ISSET(MULTIBUFFER)) {
	SET(MULTIBUFFER);
	open_buffer(tempfilename, FALSE);
	UNSET(MULTIBUFFER);
    } else
	open_buffer(tempfilename, FALSE);

    display_buffer();
}

/* Our main help-viewer function. */
void do_help(void)
{
    int kbinput = ERR;
    bool old_no_help = ISSET(NO_HELP);
    bool was_whitespace = ISSET(WHITESPACE_DISPLAY);
    int oldmenu = currmenu;
	/* The menu we were called from. */
    const char *ptr;
	/* The current line of the help text. */
    functionptrtype func;
	/* The function of the key the user typed in. */
    FILE *fp;
    int line_size;
    int saved_margin = 0;
	/* For avoiding the line numbers on the help screen. */
    char *saved_answer = (answer != NULL) ? strdup(answer) : NULL;
	/* Store current answer when user invokes help at the prompt. */

    inhelp = TRUE;

    blank_statusbar();

    /* Get a safe temporary file for displaying the help text. If we can't
     * obtain one, return. */
    tempfilename = safe_tempfile(&fp);
    fclose(fp);
    if (tempfilename == NULL) {
	statusline(ALERT, _("Error writing temp file: %s"), strerror(errno));

	inhelp = FALSE;
	free(saved_answer);
	return;
    }

    /* Set help_text as the string to display. */
    help_init();

    assert(help_text != NULL);

    if (ISSET(NO_HELP)) {
	/* Make sure that the help screen's shortcut list will actually
	 * be displayed. */
	UNSET(NO_HELP);
	window_init();
    }

    UNSET(WHITESPACE_DISPLAY);

    bottombars(MHELP);
    wnoutrefresh(bottomwin);

#ifdef ENABLE_LINENUMBERS
    if (ISSET(LINE_NUMBERS)) {
	saved_margin = margin;
	margin = 0;
	UNSET(LINE_NUMBERS);
    }
#endif

    /* Extract title from help_text and display it. */
    title = charalloc(MAX_BUF_SIZE * sizeof(char));
    ptr = help_text;
    line_size = break_line(ptr, 74, TRUE);
    strncpy(title, ptr, line_size);
    title[line_size] = '\0';
    titlebar(title);

    /* Skip the title and point to the beginning of the introductory
     * paragraphs. */
    ptr += line_size;
    while (*ptr == '\n')
	++ptr;
    beg_of_intro = ptr;

    display_the_help_text(FALSE);
    curs_set(0);

    while (TRUE) {
	edit_refresh();

	lastmessage = HUSH;
	focusing = TRUE;

	kbinput = get_kbinput(edit);

#ifndef NANO_TINY
	if (kbinput == KEY_WINCH)
	    continue;
#endif

#ifndef DISABLE_MOUSE
	if (kbinput == KEY_MOUSE) {
	    int mouse_x, mouse_y;
	    get_mouseinput(&mouse_x, &mouse_y, TRUE);
	    continue;    /* Redraw the screen. */
	}
#endif

	func = parse_help_input(&kbinput);

	if (func == total_refresh) {
	    total_redraw();
	} else if (func == do_up_void) {
	    do_up(TRUE);
	} else if (func == do_down_void) {
	    if (openfile->edittop->lineno + editwinrows < openfile->
				filebot->lineno)
		do_down(TRUE);
	} else if (func == do_page_up) {
	    do_page_up();
	} else if (func == do_page_down) {
	    do_page_down();
	} else if (func == do_first_line) {
	    do_first_line();
	} else if (func == do_last_line) {
	    do_last_line();
	} else if (func == do_search) {
	    do_search();
	    bottombars(MHELP);
	    wnoutrefresh(bottomwin);
	    curs_set(1);
	} else if (func == do_research) {
	    do_research();
	    currmenu = MHELP;
	    curs_set(1);
	} else if (func == do_exit) {
	    /* Exit from the help viewer. */
	    close_buffer();
	    break;
	} else
	    unbound_key(kbinput);
    }


    /* We're exiting from the help screen. So, restore the flags and the
     * original menu, refresh the entire screen and deallocate the memory. */

#ifdef ENABLE_LINENUMBERS
    if (saved_margin != 0) {
	margin = saved_margin;
	SET(LINE_NUMBERS);
    }
#endif

    if (old_no_help) {
	blank_bottombars();
	wnoutrefresh(bottomwin);
	currmenu = oldmenu;
	SET(NO_HELP);
	window_init();
    } else
	bottombars(oldmenu);

    if (was_whitespace)
	SET(WHITESPACE_DISPLAY);

    free(title);
    title = NULL;
    inhelp = FALSE;

#ifndef DISABLE_BROWSER
    if (oldmenu == MBROWSER || oldmenu == MWHEREISFILE || oldmenu == MGOTODIR)
	browser_refresh();
    else
#endif
	total_refresh();

    free(answer);
    answer = saved_answer;

    remove(tempfilename);
    free(tempfilename);

    free(help_text);
}

/* Allocate space for the help text for the current menu, and concatenate
 * the different pieces of text into it. */
void help_init(void)
{
    size_t allocsize = 0;
	/* Space needed for help_text. */
    const char *htx[3];
	/* Untranslated help introduction.  We break it up into three chunks
	 * in case the full string is too long for the compiler to handle. */
    char *ptr;
    const subnfunc *f;
    const sc *s;

#ifndef NANO_TINY
    bool old_whitespace = ISSET(WHITESPACE_DISPLAY);

    UNSET(WHITESPACE_DISPLAY);
#endif

    /* First, set up the initial help text for the current function. */
    if (currmenu == MWHEREIS || currmenu == MREPLACE || currmenu == MREPLACEWITH) {
	htx[0] = N_("Search Command Help Text\n\n "
		"Enter the words or characters you would like to "
		"search for, and then press Enter.  If there is a "
		"match for the text you entered, the screen will be "
		"updated to the location of the nearest match for the "
		"search string.\n\n The previous search string will be "
		"shown in brackets after the search prompt.  Hitting "
		"Enter without entering any text will perform the "
		"previous search.  ");
	htx[1] = N_("If you have selected text with the mark and then "
		"search to replace, only matches in the selected text "
		"will be replaced.\n\n The following function keys are "
		"available in Search mode:\n\n");
	htx[2] = NULL;
    } else if (currmenu == MGOTOLINE) {
	htx[0] = N_("Go To Line Help Text\n\n "
		"Enter the line number that you wish to go to and hit "
		"Enter.  If there are fewer lines of text than the "
		"number you entered, you will be brought to the last "
		"line of the file.\n\n The following function keys are "
		"available in Go To Line mode:\n\n");
	htx[1] = NULL;
	htx[2] = NULL;
    } else if (currmenu == MINSERTFILE) {
	htx[0] = N_("Insert File Help Text\n\n "
		"Type in the name of a file to be inserted into the "
		"current file buffer at the current cursor "
		"location.\n\n If you have compiled nano with multiple "
		"file buffer support, and enable multiple file buffers "
		"with the -F or --multibuffer command line flags, the "
		"Meta-F toggle, or a nanorc file, inserting a file "
		"will cause it to be loaded into a separate buffer "
		"(use Meta-< and > to switch between file buffers).  ");
	htx[1] = N_("If you need another blank buffer, do not enter "
		"any filename, or type in a nonexistent filename at "
		"the prompt and press Enter.\n\n The following "
		"function keys are available in Insert File mode:\n\n");
	htx[2] = NULL;
    } else if (currmenu == MWRITEFILE) {
	htx[0] = N_("Write File Help Text\n\n "
		"Type the name that you wish to save the current file "
		"as and press Enter to save the file.\n\n If you have "
		"selected text with the mark, you will be prompted to "
		"save only the selected portion to a separate file.  To "
		"reduce the chance of overwriting the current file with "
		"just a portion of it, the current filename is not the "
		"default in this mode.\n\n The following function keys "
		"are available in Write File mode:\n\n");
	htx[1] = NULL;
	htx[2] = NULL;
    }
#ifndef DISABLE_BROWSER
    else if (currmenu == MBROWSER) {
	htx[0] = N_("File Browser Help Text\n\n "
		"The file browser is used to visually browse the "
		"directory structure to select a file for reading "
		"or writing.  You may use the arrow keys or Page Up/"
		"Down to browse through the files, and S or Enter to "
		"choose the selected file or enter the selected "
		"directory.  To move up one level, select the "
		"directory called \"..\" at the top of the file "
		"list.\n\n The following function keys are available "
		"in the file browser:\n\n");
	htx[1] = NULL;
	htx[2] = NULL;
    } else if (currmenu == MWHEREISFILE) {
	htx[0] = N_("Browser Search Command Help Text\n\n "
		"Enter the words or characters you would like to "
		"search for, and then press Enter.  If there is a "
		"match for the text you entered, the screen will be "
		"updated to the location of the nearest match for the "
		"search string.\n\n The previous search string will be "
		"shown in brackets after the search prompt.  Hitting "
		"Enter without entering any text will perform the "
		"previous search.\n\n");
	htx[1] = N_(" The following function keys are available in "
		"Browser Search mode:\n\n");
	htx[2] = NULL;
    } else if (currmenu == MGOTODIR) {
	htx[0] = N_("Browser Go To Directory Help Text\n\n "
		"Enter the name of the directory you would like to "
		"browse to.\n\n If tab completion has not been "
		"disabled, you can use the Tab key to (attempt to) "
		"automatically complete the directory name.\n\n The "
		"following function keys are available in Browser Go "
		"To Directory mode:\n\n");
	htx[1] = NULL;
	htx[2] = NULL;
    }
#endif /* !DISABLE_BROWSER */
#ifndef DISABLE_SPELLER
    else if (currmenu == MSPELL) {
	htx[0] = N_("Spell Check Help Text\n\n "
		"The spell checker checks the spelling of all text in "
		"the current file.  When an unknown word is "
		"encountered, it is highlighted and a replacement can "
		"be edited.  It will then prompt to replace every "
		"instance of the given misspelled word in the current "
		"file, or, if you have selected text with the mark, in "
		"the selected text.\n\n The following function keys "
		"are available in Spell Check mode:\n\n");
	htx[1] = NULL;
	htx[2] = NULL;
    }
#endif /* !DISABLE_SPELLER */
#ifndef NANO_TINY
    else if (currmenu == MEXTCMD) {
	htx[0] = N_("Execute Command Help Text\n\n "
		"This mode allows you to insert the output of a "
		"command run by the shell into the current buffer (or "
		"a new buffer in multiple file buffer mode).  If you "
		"need another blank buffer, do not enter any "
		"command.\n\n The following function keys are "
		"available in Execute Command mode:\n\n");
	htx[1] = NULL;
	htx[2] = NULL;
    }
#endif /* !NANO_TINY */
    else {
	/* Default to the main help list. */
	htx[0] = N_("Main nano help text\n\n "
		"The nano editor is designed to emulate the "
		"functionality and ease-of-use of the UW Pico text "
		"editor.  There are four main sections of the editor.  "
		"The top line shows the program version, the current "
		"filename being edited, and whether or not the file "
		"has been modified.  Next is the main editor window "
		"showing the file being edited.  The status line is "
		"the third line from the bottom and shows important "
		"messages.  ");
	htx[1] = N_("The bottom two lines show the most commonly used "
		"shortcuts in the editor.\n\n Shortcuts are written as "
		"follows: Control-key sequences are notated with a '^' "
		"and can be entered either by using the Ctrl key or "
		"pressing the Esc key twice.  Meta-key sequences are "
		"notated with 'M-' and can be entered using either the "
		"Alt, Cmd, or Esc key, depending on your keyboard setup.  ");
	htx[2] = N_("Also, pressing Esc twice and then typing a "
		"three-digit decimal number from 000 to 255 will enter "
		"the character with the corresponding value.  The "
		"following keystrokes are available in the main editor "
		"window.  Alternative keys are shown in "
		"parentheses:\n\n");
    }

    htx[0] = _(htx[0]);
    if (htx[1] != NULL)
	htx[1] = _(htx[1]);
    if (htx[2] != NULL)
	htx[2] = _(htx[2]);

    allocsize += strlen(htx[0]);
    if (htx[1] != NULL)
	allocsize += strlen(htx[1]);
    if (htx[2] != NULL)
	allocsize += strlen(htx[2]);

    /* Calculate the length of the shortcut help text.  Each entry has
     * one or two keys, which fill 16 columns, plus translated text,
     * plus one or two \n's. */
    for (f = allfuncs; f != NULL; f = f->next)
	if (f->menus & currmenu)
	    allocsize += (16 * MAXCHARLEN) + strlen(f->help) + 2;

#ifndef NANO_TINY
    /* If we're on the main list, we also count the toggle help text.
     * Each entry has "M-%c\t\t", five chars which fill 16 columns,
     * plus a space, plus translated text, plus one or two '\n's. */
    if (currmenu == MMAIN) {
	size_t endis_len = strlen(_("enable/disable"));

	for (s = sclist; s != NULL; s = s->next)
	    if (s->scfunc == do_toggle_void)
		allocsize += strlen(_(flagtostr(s->toggle))) + endis_len + 8;
    }
#endif

    /* Allocate space for the help text. */
    help_text = charalloc(allocsize + 1);

    /* Now add the text we want. */
    strcpy(help_text, htx[0]);
    if (htx[1] != NULL)
	strcat(help_text, htx[1]);
    if (htx[2] != NULL)
	strcat(help_text, htx[2]);

    ptr = help_text + strlen(help_text);

    /* Remember this end-of-introduction, start-of-shortcuts. */
    end_of_intro = ptr;

    /* Now add our shortcut info. */
    for (f = allfuncs; f != NULL; f = f->next) {
	int scsfound = 0;

	if ((f->menus & currmenu) == 0)
	    continue;

	/* Let's simply show the first two shortcuts from the list. */
	for (s = sclist; s != NULL; s = s->next) {

	    if ((s->menus & currmenu) == 0)
		continue;

	    if (s->scfunc == f->scfunc) {
		scsfound++;
		/* Make the first column narrower (6) than the second (10),
		 * but allow it to spill into the second, for "M-Space". */
		if (scsfound == 1) {
		    sprintf(ptr, "%s               ", s->keystr);
		    /* Unicode arrows take three bytes instead of one. */
		    if (s->keystr[0] == '\xE2' || s->keystr[1] == '\xE2')
			ptr += 8;
		    else
			ptr += 6;
		} else {
		    ptr += sprintf(ptr, "(%s)\t", s->keystr);
		    break;
		}
	    }
	}

	if (scsfound == 0)
	    ptr += sprintf(ptr, "\t\t");
	else if (scsfound == 1)
	    ptr += 10;

	/* The shortcut's help text. */
	ptr += sprintf(ptr, "%s\n", _(f->help));

	if (f->blank_after)
	    ptr += sprintf(ptr, "\n");
    }

#ifndef NANO_TINY
    /* And the toggles... */
    if (currmenu == MMAIN) {
	int maximum = 0, counter = 0;

	/* First see how many toggles there are. */
	for (s = sclist; s != NULL; s = s->next)
	    maximum = (s->toggle && s->ordinal > maximum) ? s->ordinal : maximum;

	/* Now show them in the original order. */
	while (counter < maximum) {
	    counter++;
	    for (s = sclist; s != NULL; s = s->next)
		if (s->toggle && s->ordinal == counter) {
		    ptr += sprintf(ptr, "%s\t\t%s %s\n", (s->menus == MMAIN ? s->keystr : ""),
				_(flagtostr(s->toggle)), _("enable/disable"));
		    if (s->toggle == NO_COLOR_SYNTAX || s->toggle == TABS_TO_SPACES)
			ptr += sprintf(ptr, "\n");
		    break;
		}
	}
    }

    if (old_whitespace)
	SET(WHITESPACE_DISPLAY);
#endif /* !NANO_TINY */

    /* If all went well, we didn't overwrite the allocated space. */
    assert(strlen(help_text) <= allocsize + 1);
}

/* Return the function that is bound to the given key, accepting certain
 * plain characters too, for consistency with the file browser. */
functionptrtype parse_help_input(int *kbinput)
{
    if (!meta_key) {
	switch (*kbinput) {
	    case ' ':
		return do_page_down;
	    case '-':
		return do_page_up;
	    case 'E':
	    case 'e':
	    case 'Q':
	    case 'q':
	    case 'X':
	    case 'x':
		return do_exit;
	}
    }
    return func_from_key(kbinput);
}

/* Calculate the displayable length of the help-text line starting at ptr. */
size_t help_line_len(const char *ptr)
{
    size_t wrapping_point = (COLS > 24) ? COLS - 1 : 24;
	/* The target width for wrapping long lines. */
    ssize_t wrap_location;
	/* Actual position where the line can be wrapped. */
    size_t length = 0;
	/* Full length of the line, until the first newline. */

    /* Avoid overwide paragraphs in the introductory text. */
    if (ptr < end_of_intro && COLS > 74)
	wrapping_point = 74;

    wrap_location = break_line(ptr, wrapping_point, TRUE);

    /* Get the length of the entire line up to a null or a newline. */
    while (*(ptr + length) != '\0' && *(ptr + length) != '\n')
	length = move_mbright(ptr, length);

    /* If the entire line will just fit the screen, don't wrap it. */
    if (strnlenpt(ptr, length) <= wrapping_point + 1)
	return length;
    else if (wrap_location > 0)
	return wrap_location;
    else
	return 0;
}

#endif /* !DISABLE_HELP */

/* Start the help viewer. */
void do_help_void(void)
{
#ifndef DISABLE_HELP
    do_help();
#else
    if (currmenu == MMAIN)
	say_there_is_no_help();
    else
	beep();
#endif /* !DISABLE_HELP */
}
