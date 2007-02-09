/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of lv2dynparam libraries
 *
 *   Copyright (C) 2006,2007 Nedko Arnaudov <nedko@arnaudov.name>
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

#include "types.h"
#include "memory_atomic.h"
#include "helpers.h"

//#define LOG_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

char **
lv2dynparam_enum_duplicate(
  lv2dynparam_memory_handle memory,
  const char * const * values_ptr_ptr,
  unsigned int values_count)
{
  unsigned int i;
  char ** values;
  size_t value_size;

  LOG_DEBUG("Duplicating enum array of %u elements", values_count);
  for (i = 0 ; i < values_count ; i++)
  {
    LOG_DEBUG("\"%s\"", values_ptr_ptr[i]);
  }

  values = lv2dynparam_memory_allocate(memory, values_count * sizeof(char *));
  if (values == NULL)
  {
    LOG_DEBUG("Failed to allocate memory for enum array");
    return NULL;
  }

  for (i = 0 ; i < values_count ; i++)
  {
    value_size = strlen(values_ptr_ptr[i]) + 1;

    LOG_DEBUG("Allocating memory for enum array entry at index %u", i);
    values[i] = lv2dynparam_memory_allocate(memory, value_size);
    if (values[i] == NULL)
    {
      LOG_DEBUG("Failed to allocate memory for enum array entry");

      while (i > 0)
      {
        i--;
        LOG_DEBUG("Deallocating memory for enum array entry at index %u", i);
        lv2dynparam_memory_deallocate(values[i]);
      }

      lv2dynparam_memory_deallocate(values);

      return NULL;
    }

    memcpy(values[i], values_ptr_ptr[i], value_size);
  }

  LOG_DEBUG("Duplicated enum array of %u elements", values_count);
  for (i = 0 ; i < values_count ; i++)
  {
    LOG_DEBUG("\"%s\"", values[i]);
  }

  return values;
}
