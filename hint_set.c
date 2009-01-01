/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of lv2dynparam libraries
 *
 *   Copyright (C) 2007,2008,2009 Nedko Arnaudov <nedko@arnaudov.name>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; version 2 of the License
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *****************************************************************************/

#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <stdbool.h>

#include "lv2.h"
#include "lv2dynparam.h"
#include "memory_atomic.h"
#include "hint_set.h"
#include "helpers.h"

void
lv2dynparam_hints_init_empty(
  struct lv2dynparam_hints * hints_ptr)
{
  hints_ptr->count = 0;
  hints_ptr->names = NULL;
  hints_ptr->values = NULL;
}

bool
lv2dynparam_hints_init_va_link(
  rtsafe_memory_handle memory,
  struct lv2dynparam_hints * hints_ptr,
  ...)
{
  va_list ap;
  unsigned int index;
  unsigned int count;
  const char * arg;

  /* iterate varargs and compute name/value pairs */

  va_start(ap, hints_ptr);

  count = 0;
  while (va_arg(ap, char *) != NULL)
  {
    arg = va_arg(ap, const char *);
    count++;
  }

  va_end(ap);

  hints_ptr->count = count;

  if (count == 0)
  {
    hints_ptr->names = NULL;
    hints_ptr->values = NULL;
    return true;
  }

  /* allocate names and values arrays */

  hints_ptr->names = rtsafe_memory_allocate(memory, count * sizeof(char *));
  if (hints_ptr->names == NULL)
  {
    return false;
  }

  hints_ptr->values = rtsafe_memory_allocate(memory, count * sizeof(char *));
  if (hints_ptr->values == NULL)
  {
    rtsafe_memory_deallocate(hints_ptr->names);
    return false;
  }

  /* fill the names and values arrays */

  va_start(ap, hints_ptr);
  index = 0;
loop:
  hints_ptr->names[index] = va_arg(ap, char *);
  if (hints_ptr->names[index] == NULL)
  {
    va_end(ap);
    return true;
  }

  hints_ptr->values[index] = va_arg(ap, char *);

  index++;
  goto loop;
}

bool
lv2dynparam_hints_init_va_dup(
  rtsafe_memory_handle memory,
  struct lv2dynparam_hints * hints_ptr,
  ...)
{
  va_list ap;
  unsigned int index;
  unsigned int count;
  char * name;
  char * value;

  /* iterate varargs and compute name/value pairs */

  va_start(ap, hints_ptr);

  count = 0;
  while (va_arg(ap, char *) != NULL)
  {
    value = va_arg(ap, char *);
    count++;
  }

  va_end(ap);

  hints_ptr->count = count;

  if (count == 0)
  {
    hints_ptr->names = NULL;
    hints_ptr->values = NULL;
    return true;
  }

  /* allocate names and values arrays */

  hints_ptr->names = rtsafe_memory_allocate(memory, count * sizeof(char *));
  if (hints_ptr->names == NULL)
  {
    goto fail;
  }

  hints_ptr->values = rtsafe_memory_allocate(memory, count * sizeof(char *));
  if (hints_ptr->values == NULL)
  {
    goto fail_deallocate_names_array;
  }

  /* fill the names and values arrays */

  va_start(ap, hints_ptr);
  index = 0;

loop:
  assert(index < count);

  name = va_arg(ap, char *);

  if (name == NULL)
  {
    va_end(ap);
    return true;
  }

  hints_ptr->names[index] = lv2dynparam_strdup_atomic(memory, name);
  if (hints_ptr->names[index] == NULL)
  {
    goto fail_cleanup_arrays;
  }

  value = va_arg(ap, char *);
  if (value == NULL)
  {
    hints_ptr->values[index] = NULL;
  }
  else
  {
    hints_ptr->values[index] = lv2dynparam_strdup_atomic(memory, value);
    if (hints_ptr->values[index] == NULL)
    {
      rtsafe_memory_deallocate(hints_ptr->names[index]);
      goto fail_cleanup_arrays;
    }
  }

  index++;
  goto loop;

fail_cleanup_arrays:
  va_end(ap);

  while (index > 0)
  {
    index--;

    rtsafe_memory_deallocate(hints_ptr->names[index]);

    if (hints_ptr->values[index] != NULL)
    {
      rtsafe_memory_deallocate(hints_ptr->values[index]);
    }
  }

  rtsafe_memory_deallocate(hints_ptr->values);

fail_deallocate_names_array:
  rtsafe_memory_deallocate(hints_ptr->names);

fail:
  return false;
}

bool
lv2dynparam_hints_init_copy(
  rtsafe_memory_handle memory,
  const struct lv2dynparam_hints * src_hints_ptr,
  struct lv2dynparam_hints * dest_hints_ptr)
{
  unsigned int index;

  dest_hints_ptr->count = src_hints_ptr->count;

  if (dest_hints_ptr->count == 0)
  {
    dest_hints_ptr->names = NULL;
    dest_hints_ptr->values = NULL;
    return true;
  }

  /* allocate names and values arrays */

  dest_hints_ptr->names = rtsafe_memory_allocate(memory, src_hints_ptr->count * sizeof(char *));
  if (dest_hints_ptr->names == NULL)
  {
    goto fail;
  }

  dest_hints_ptr->values = rtsafe_memory_allocate(memory, src_hints_ptr->count * sizeof(char *));
  if (dest_hints_ptr->values == NULL)
  {
    goto fail_deallocate_names_array;
  }

  /* fill the names and values arrays */

  for (index = 0 ; index < src_hints_ptr->count ; index++)
  {
    dest_hints_ptr->names[index] = lv2dynparam_strdup_atomic(memory, src_hints_ptr->names[index]);
    if (dest_hints_ptr->names[index] == NULL)
    {
      goto fail_cleanup_arrays;
    }

    if (src_hints_ptr->values[index] == NULL)
    {
      dest_hints_ptr->values[index] = NULL;
    }
    else
    {
      dest_hints_ptr->values[index] = lv2dynparam_strdup_atomic(memory, src_hints_ptr->values[index]);
      if (dest_hints_ptr->values[index] == NULL)
      {
        rtsafe_memory_deallocate(dest_hints_ptr->names[index]);
        goto fail_cleanup_arrays;
      }
    }
  }

  return true;

fail_cleanup_arrays:
  while (index > 0)
  {
    index--;

    rtsafe_memory_deallocate(dest_hints_ptr->names[index]);

    if (dest_hints_ptr->values[index] != NULL)
    {
      rtsafe_memory_deallocate(dest_hints_ptr->values[index]);
    }
  }

  rtsafe_memory_deallocate(dest_hints_ptr->values);

fail_deallocate_names_array:
  rtsafe_memory_deallocate(dest_hints_ptr->names);

fail:
  return false;
}

void
lv2dynparam_hints_clear(
  struct lv2dynparam_hints * hints_ptr)
{
  while (hints_ptr->count > 0)
  {
    hints_ptr->count--;

    rtsafe_memory_deallocate(hints_ptr->names[hints_ptr->count]);

    if (hints_ptr->values[hints_ptr->count] != NULL)
    {
      rtsafe_memory_deallocate(hints_ptr->values[hints_ptr->count]);
    }
  }

  if (hints_ptr->names != NULL)
  {
    rtsafe_memory_deallocate(hints_ptr->names);
  }

  if (hints_ptr->values != NULL)
  {
    rtsafe_memory_deallocate(hints_ptr->values);
  }
}
