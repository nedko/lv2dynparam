/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of lv2dynparam libraries
 *
 *   Copyright (C) 2006,2007,2008,2009 Nedko Arnaudov <nedko@arnaudov.name>
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
#include <string.h>
#include <stdbool.h>

#include "memory_atomic.h"
#include "helpers.h"

//#define LOG_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

char *
lv2dynparam_strdup_atomic(
  rtsafe_memory_handle memory,
  const char * source)
{
  size_t size;
  char * dest;

  size = strlen(source) + 1;

  dest = rtsafe_memory_allocate(memory, size);
  if (dest == NULL)
  {
    return NULL;
  }

  memcpy(dest, source, size);

  return dest;
}

char *
lv2dynparam_strdup_sleepy(
  rtsafe_memory_handle memory,
  const char * source)
{
  size_t size;
  char * dest;

  size = strlen(source) + 1;

  dest = rtsafe_memory_allocate_sleepy(memory, size);
  if (dest == NULL)
  {
    return NULL;
  }

  memcpy(dest, source, size);

  return dest;
}

char **
lv2dynparam_enum_duplicate(
  rtsafe_memory_handle memory,
  const char * const * values_ptr_ptr,
  unsigned int values_count)
{
  unsigned int i;
  char ** values;

  LOG_DEBUG("Duplicating enum array of %u elements", values_count);
  for (i = 0 ; i < values_count ; i++)
  {
    LOG_DEBUG("\"%s\"", values_ptr_ptr[i]);
  }

  values = rtsafe_memory_allocate(memory, values_count * sizeof(char *));
  if (values == NULL)
  {
    LOG_DEBUG("Failed to allocate memory for enum array");
    return NULL;
  }

  for (i = 0 ; i < values_count ; i++)
  {
    LOG_DEBUG("Allocating memory for enum array entry at index %u", i);
    values[i] = lv2dynparam_strdup_atomic(memory, values_ptr_ptr[i]);
    if (values[i] == NULL)
    {
      LOG_DEBUG("Failed to allocate memory for enum array entry");

      while (i > 0)
      {
        i--;
        LOG_DEBUG("Deallocating memory for enum array entry at index %u", i);
        rtsafe_memory_deallocate(values[i]);
      }

      rtsafe_memory_deallocate(values);

      return NULL;
    }
  }

  LOG_DEBUG("Duplicated enum array of %u elements", values_count);
  for (i = 0 ; i < values_count ; i++)
  {
    LOG_DEBUG("\"%s\"", values[i]);
  }

  return values;
}

void
lv2dynparam_enum_free(
  rtsafe_memory_handle memory,
  char ** values,
  unsigned int values_count)
{
  while (values_count > 0)
  {
    values_count--;

    rtsafe_memory_deallocate(values[values_count]);
  }

  rtsafe_memory_deallocate(values);
}
