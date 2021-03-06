/* Macro facility functions

   Copyright (c) 2008 Free Software Foundation, Inc.
   Copyright (c) 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004 Sandro Sigala.
   Copyright (c) 2005 Reuben Thomas.

   This file is part of GNU Zile.

   GNU Zile is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GNU Zile is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Zile; see the file COPYING.  If not, write to the
   Free Software Foundation, Fifth Floor, 51 Franklin Street, Boston,
   MA 02111-1301, USA.  */

#include "config.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "size_max.h"

#include "zile.h"
#include "extern.h"


static Macro *cur_mp, *cmd_mp = NULL, *head_mp = NULL;

static void
macro_delete (Macro * mp)
{
  if (mp)
    {
      free (mp->keys);
      free (mp);
    }
}

static void
add_macro_key (Macro * mp, size_t key)
{
  mp->keys = xrealloc (mp->keys, sizeof (size_t) * ++mp->nkeys);
  mp->keys[mp->nkeys - 1] = key;
}

void
add_cmd_to_macro (void)
{
  size_t i;
  assert (cmd_mp);
  for (i = 0; i < cmd_mp->nkeys; i++)
    add_macro_key (cur_mp, cmd_mp->keys[i]);
  macro_delete (cmd_mp);
  cmd_mp = NULL;
}

void
add_key_to_cmd (size_t key)
{
  if (cmd_mp == NULL)
    cmd_mp = XZALLOC (Macro);

  add_macro_key (cmd_mp, key);
}

void
cancel_kbd_macro (void)
{
  macro_delete (cmd_mp);
  macro_delete (cur_mp);
  cmd_mp = cur_mp = NULL;
  thisflag &= ~FLAG_DEFINING_MACRO;
}

DEFUN ("start-kbd-macro", start_kbd_macro)
/*+
Record subsequent keyboard input, defining a keyboard macro.
The commands are recorded even as they are executed.
Use C-x ) to finish recording and make the macro available.
Use M-x name-last-kbd-macro to give it a permanent name.
+*/
{
  if (thisflag & FLAG_DEFINING_MACRO)
    {
      minibuf_error ("Already defining a keyboard macro");
      return leNIL;
    }

  if (cur_mp)
    cancel_kbd_macro ();

  minibuf_write ("Defining keyboard macro...");

  thisflag |= FLAG_DEFINING_MACRO;
  cur_mp = XZALLOC (Macro);
  return leT;
}
END_DEFUN

DEFUN ("end-kbd-macro", end_kbd_macro)
/*+
Finish defining a keyboard macro.
The definition was started by C-x (.
The macro is now available for use via C-x e.
+*/
{
  if (!(thisflag & FLAG_DEFINING_MACRO))
    {
      minibuf_error ("Not defining a keyboard macro");
      return leNIL;
    }

  thisflag &= ~FLAG_DEFINING_MACRO;
  return leT;
}
END_DEFUN

DEFUN ("name-last-kbd-macro", name_last_kbd_macro)
/*+
Assign a name to the last keyboard macro defined.
Argument SYMBOL is the name to define.
The symbol's function definition becomes the keyboard macro string.
Such a "function" cannot be called from Lisp, but it is a valid editor command.
+*/
{
  Macro *mp;
  size_t size;
  char *ms = minibuf_read ("Name for last kbd macro: ", "");

  if (ms == NULL)
    {
      minibuf_error ("No command name given");
      return leNIL;
    }

  if (cur_mp == NULL)
    {
      minibuf_error ("No keyboard macro defined");
      return leNIL;
    }

  mp = get_macro (ms);
  if (mp)
    {
      /* If a macro with this name already exists, update its key list */
      free (mp->keys);
    }
  else
    {
      /* Add a new macro to the list */
      mp = XZALLOC (Macro);
      mp->next = head_mp;
      mp->name = xstrdup (ms);
      head_mp = mp;
    }

  /* Copy the keystrokes from cur_mp. */
  mp->nkeys = cur_mp->nkeys;
  size = sizeof (*(mp->keys)) * mp->nkeys;
  mp->keys = xzalloc (size);
  memcpy (mp->keys, cur_mp->keys, size);

  return leT;
}
END_DEFUN

void
call_macro (Macro * mp)
{
  size_t i;

  for (i = mp->nkeys - 1; i != SIZE_MAX; i--)
    ungetkey (mp->keys[i]);
}

DEFUN ("call-last-kbd-macro", call_last_kbd_macro)
/*+
Call the last keyboard macro that you defined with C-x (.
A prefix argument serves as a repeat count.

To make a macro permanent so you can call it even after
defining others, use M-x name-last-kbd-macro.
+*/
{
  int uni;
  le *ret = leT;

  if (cur_mp == NULL)
    {
      minibuf_error ("No kbd macro has been defined");
      return leNIL;
    }

  undo_save (UNDO_START_SEQUENCE, cur_bp->pt, 0, 0);
  for (uni = 0; uni < uniarg; ++uni)
    call_macro (cur_mp);
  undo_save (UNDO_END_SEQUENCE, cur_bp->pt, 0, 0);

  return ret;
}
END_DEFUN

/*
 * Free all the macros (used at Zile exit).
 */
void
free_macros (void)
{
  Macro *mp, *next;

  macro_delete (cur_mp);

  for (mp = head_mp; mp; mp = next)
    {
      next = mp->next;
      macro_delete (mp);
    }
}

/*
 * Find a macro given its name.
 */
Macro *
get_macro (const char *name)
{
  Macro *mp;
  assert (name);
  for (mp = head_mp; mp; mp = mp->next)
    if (!strcmp (mp->name, name))
      return mp;
  return NULL;
}
