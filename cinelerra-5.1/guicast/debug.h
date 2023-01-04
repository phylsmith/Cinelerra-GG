/*
 * CINELERRA
 * Copyright (C) 2020 William Morrow
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


#ifndef DEBUG_H
#define DEBUG_H

#undef DEBUG
#ifdef DEBUG

#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* Debug macros turned on/off using the environment variable DEBUG.
   Set this from the command line as 'DEBUG=foo,bar cinelerra'. Examples:
     DEBUG=file.C      -> all debug statements in file.C
     DEBUG=func        -> all in functions named 'func'
     DEBUG=func*       -> all in functions (or files) starting with 'func'
     DEBUG=file*       -> all in files (or functions) starting with 'file'
     DEBUG=func1,func2 -> either in func1 or in func2
     DEBUG="func, file.C" -> starting with func or in file.C
     DEBUG=*           -> just print all debug statements
   Wildcard character '*' can only go at the end of an identifier.
   Whitespace after comma is allowed, but before comma is bad.
   Printing can also be controlled at compile time using DEBUG_PRINT_ON/OFF.
   Code for debug statements is only compiled "#ifdef DEBUG" at compile.
*/

// NOTE: gcc drops '~' from destructors in __func__, so ~FOO becomes FOO

static int debug_print_all = 0;

static int debug_should_print(const char *file,
			      const char *func)
{
	if (debug_print_all) return 1;

	char *debug = getenv("DEBUG");
	if (! debug) return 0;

	char *next = debug;
	for (char *test = debug; next != NULL; test = next + 1) {
		next = strchr(test, ',');
		int length = next ? next - test - 1 : strlen(test) - 1;

		if (test[length] == '*') {
			if (! strncmp(test, file, length)) return 1;
			if (! strncmp(test, func, length)) return 1;
		}
		else {
			if (! strncmp(test, file, strlen(file))) return 1;
			if (! strncmp(test, func, strlen(func))) return 1;
		}

		if (next) while(isspace(*next)) next++;
	}

	return 0;
}

#define DEBUG_PRINT_ON() debug_print_all = 1
#define DEBUG_PRINT_OFF() debug_print_all = 0
#define DEBUG_PRINT(format, args...)                                   \
    printf("%s:%d %s(): " format "\n", __FILE__, __LINE__, __func__, ## args)


// assert debug warning if test fails
#define ADEBUG(test, args...)                                          \
    if (debug_should_print(__FILE__, __func__)) { \
            if (! test) DEBUG_PRINT("ASSERT FAILED (" #test ") " args) \
    }

// do debug statements
#define DDEBUG(actions...)                                             \
    if (debug_should_print(__FILE__, __func__)) { \
            actions;                                                   \
    }

// print debug statement
#define PDEBUG(format, args...)                                        \
    if (debug_should_print(__FILE__, __func__)) { \
            DEBUG_PRINT(format, ## args);                              \
    }

// this debug statement (PDEBUG including %p this)
#define TDEBUG(format, args...)                                        \
    if (debug_should_print(__FILE__, __func__)) { \
	    DEBUG_PRINT("%p " format, this, ##args);                   \
    }

#else  /* not DEBUG */

#define DEBUG_ON()
#define DEBUG_OFF()
#define ADEBUG(test, args...)
#define DDEBUG(actions...)
#define PDEBUG(format, args...)
#define TDEBUG(format, args...)

#endif /* DEBUG */

#endif /* DEBUG_H */
