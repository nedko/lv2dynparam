/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of lv2dynparam plugin library
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

#include <string.h>
#include <assert.h>

#include "../lv2.h"
#include "../lv2dynparam.h"
#include "plugin.h"
#include "../list.h"
#include "../memory_atomic.h"
#include "internal.h"
#include "../helpers.h"
#include "../hint_set.h"

//#define LOG_LEVEL LOG_LEVEL_DEBUG
#include "../log.h"

void
lv2dynparam_plugin_parameter_free(
  struct lv2dynparam_plugin_instance * instance_ptr,
  struct lv2dynparam_plugin_parameter * param_ptr)
{
  LOG_DEBUG("Freeing parameter \"%s\"", param_ptr->name);

  switch (param_ptr->type)
  {
  case LV2DYNPARAM_PARAMETER_TYPE_ENUM:
    lv2dynparam_enum_free(
      instance_ptr->memory,
      param_ptr->data.enumeration.values,
      param_ptr->data.enumeration.values_count);
    return;
  }

  lv2dynparam_hints_clear(&param_ptr->hints);
  lv2dynparam_memory_pool_deallocate(instance_ptr->parameters_pool, param_ptr);
}

#define parameter_ptr ((struct lv2dynparam_plugin_parameter *)parameter)

void
lv2dynparam_plugin_parameter_get_type_uri(
  lv2dynparam_parameter_handle parameter,
  char * buffer)
{
  size_t s;
  const char * uri;

  switch (parameter_ptr->type)
  {
  case LV2DYNPARAM_PARAMETER_TYPE_FLOAT:
    uri = LV2DYNPARAM_PARAMETER_TYPE_FLOAT_URI;
    break;
  case LV2DYNPARAM_PARAMETER_TYPE_INT:
    uri = LV2DYNPARAM_PARAMETER_TYPE_INT_URI;
    break;
  case LV2DYNPARAM_PARAMETER_TYPE_NOTE:
    uri = LV2DYNPARAM_PARAMETER_TYPE_NOTE_URI;
    break;
  case LV2DYNPARAM_PARAMETER_TYPE_STRING:
    uri = LV2DYNPARAM_PARAMETER_TYPE_STRING_URI;
    break;
  case LV2DYNPARAM_PARAMETER_TYPE_FILENAME:
    uri = LV2DYNPARAM_PARAMETER_TYPE_FILENAME_URI;
    break;
  case LV2DYNPARAM_PARAMETER_TYPE_BOOLEAN:
    uri = LV2DYNPARAM_PARAMETER_TYPE_BOOLEAN_URI;
    break;
  case LV2DYNPARAM_PARAMETER_TYPE_ENUM:
    uri = LV2DYNPARAM_PARAMETER_TYPE_ENUM_URI;
    break;
  default:
    assert(0);
    return;
  }

  s = strlen(uri) + 1;

  assert(s <= LV2DYNPARAM_MAX_STRING_SIZE);

  memcpy(buffer, uri, s);
}

void
lv2dynparam_plugin_parameter_get_name(
  lv2dynparam_parameter_handle parameter,
  char * buffer)
{
  size_t s;

  s = strlen(parameter_ptr->name) + 1;

  assert(s <= LV2DYNPARAM_MAX_STRING_SIZE);

  memcpy(buffer, parameter_ptr->name, s);
}

void
lv2dynparam_plugin_parameter_get_value(
  lv2dynparam_parameter_handle parameter,
  void ** value_buffer)
{
  switch (parameter_ptr->type)
  {
  case LV2DYNPARAM_PARAMETER_TYPE_FLOAT:
    *value_buffer = &parameter_ptr->data.fpoint.value;
    return;
  case LV2DYNPARAM_PARAMETER_TYPE_INT:
    *value_buffer = &parameter_ptr->data.integer.value;
    return;
  case LV2DYNPARAM_PARAMETER_TYPE_NOTE:
    *value_buffer = &parameter_ptr->data.note.value;
    return;
  case LV2DYNPARAM_PARAMETER_TYPE_STRING:
    return;
  case LV2DYNPARAM_PARAMETER_TYPE_FILENAME:
    return;
  case LV2DYNPARAM_PARAMETER_TYPE_BOOLEAN:
    *value_buffer = &parameter_ptr->data.boolean;
    return;
  case LV2DYNPARAM_PARAMETER_TYPE_ENUM:
    *value_buffer = &parameter_ptr->data.enumeration.selected_value;
    return;
  }
}

void
lv2dynparam_plugin_parameter_get_range(
  lv2dynparam_parameter_handle parameter,
  void ** value_min_buffer,
  void ** value_max_buffer)
{
  switch (parameter_ptr->type)
  {
  case LV2DYNPARAM_PARAMETER_TYPE_FLOAT:
    *value_min_buffer = &parameter_ptr->data.fpoint.min;
    *value_max_buffer = &parameter_ptr->data.fpoint.max;
    return;
  case LV2DYNPARAM_PARAMETER_TYPE_INT:
    *value_min_buffer = &parameter_ptr->data.integer.min;
    *value_max_buffer = &parameter_ptr->data.integer.max;
    return;
  case LV2DYNPARAM_PARAMETER_TYPE_NOTE:
    *value_min_buffer = &parameter_ptr->data.note.min;
    *value_max_buffer = &parameter_ptr->data.note.max;
    return;
  case LV2DYNPARAM_PARAMETER_TYPE_ENUM:
    *value_min_buffer = &parameter_ptr->data.enumeration.values;
    *value_max_buffer = &parameter_ptr->data.enumeration.values_count;
    return;
  }
}

unsigned char
lv2dynparam_plugin_parameter_change(
  lv2dynparam_parameter_handle parameter)
{
  //LOG_DEBUG("lv2dynparam_plugin_parameter_change() called.");

  switch (parameter_ptr->type)
  {
  case LV2DYNPARAM_PARAMETER_TYPE_FLOAT:
    if (!parameter_ptr->plugin_callback.fpoint(
          parameter_ptr->plugin_callback_context,
          parameter_ptr->data.fpoint.value))
    {
      return FALSE;
    }

    return TRUE;

  case LV2DYNPARAM_PARAMETER_TYPE_INT:
    return TRUE;

  case LV2DYNPARAM_PARAMETER_TYPE_NOTE:
    return TRUE;

  case LV2DYNPARAM_PARAMETER_TYPE_STRING:
    return TRUE;

  case LV2DYNPARAM_PARAMETER_TYPE_FILENAME:
    return TRUE;

  case LV2DYNPARAM_PARAMETER_TYPE_BOOLEAN:
    if (!parameter_ptr->plugin_callback.boolean(
          parameter_ptr->plugin_callback_context,
          parameter_ptr->data.boolean))
    {
      return FALSE;
    }

    return TRUE;

  case LV2DYNPARAM_PARAMETER_TYPE_ENUM:
    if (!parameter_ptr->plugin_callback.enumeration(
          parameter_ptr->plugin_callback_context,
          parameter_ptr->data.enumeration.values[parameter_ptr->data.enumeration.selected_value],
          parameter_ptr->data.enumeration.selected_value))
    {
      return FALSE;
    }

    return TRUE;

  default:
    assert(0);
  }

  return TRUE;
}

void
lv2dynparam_plugin_param_notify(
  struct lv2dynparam_plugin_instance * instance_ptr,
  struct lv2dynparam_plugin_parameter * param_ptr)
{
  if (instance_ptr->host_callbacks == NULL)
  {
    /* Host not attached */
    return;
  }

  switch (param_ptr->pending)
  {
  case LV2DYNPARAM_PENDING_NOTHING:
    /* There is nothing to notify for */
    return;
  case LV2DYNPARAM_PENDING_APPEAR:
/*     LOG_DEBUG("Appearing %s", param_ptr->name); */
    if (instance_ptr->host_callbacks->parameter_appear(
          instance_ptr->host_context,
          param_ptr->group_ptr->host_context,
          param_ptr,
          &param_ptr->hints,
          &param_ptr->host_context))
    {
      param_ptr->pending = LV2DYNPARAM_PENDING_NOTHING;
      instance_ptr->pending--;
    }
    return;
  case LV2DYNPARAM_PENDING_DISAPPEAR:
/*     LOG_DEBUG("Disappering %s", param_ptr->name); */
    if (instance_ptr->host_callbacks->parameter_disappear(
          instance_ptr->host_context,
          param_ptr->host_context))
    {
      param_ptr->pending = LV2DYNPARAM_PENDING_NOTHING;
      list_del(&param_ptr->siblings);
      instance_ptr->pending--;
    }
    return;
  default:
    assert(0);
  }
}

#define instance_ptr ((struct lv2dynparam_plugin_instance *)instance_handle)

BOOL
lv2dynparam_plugin_param_boolean_add(
  lv2dynparam_plugin_instance instance_handle,
  lv2dynparam_plugin_group group,
  const char * name,
  int value,
  lv2dynparam_plugin_param_boolean_changed callback,
  void * callback_context,
  lv2dynparam_plugin_parameter * param_handle_ptr)
{
  struct lv2dynparam_plugin_parameter * param_ptr;
  struct lv2dynparam_plugin_group * group_ptr;
  struct list_head * node_ptr;
  size_t name_size;

  LOG_DEBUG("lv2dynparam_plugin_param_boolean_add() called for \"%s\"", name);

  name_size = strlen(name) + 1;
  if (name_size >= LV2DYNPARAM_MAX_STRING_SIZE)
  {
    assert(0);
    return FALSE;
  }

  if (group == NULL)
  {
    group_ptr = &instance_ptr->root_group;
  }
  else
  {
    group_ptr = (struct lv2dynparam_plugin_group *)group;
  }

  /* Search for same parameter in pending disappear state, and try to reuse it */
  list_for_each(node_ptr, &group_ptr->child_parameters)
  {
    param_ptr = list_entry(node_ptr, struct lv2dynparam_plugin_parameter, siblings);

    assert(param_ptr->group_ptr == group_ptr);

    if (strcmp(param_ptr->name, name) == 0)
    {
      if (param_ptr->pending != LV2DYNPARAM_PENDING_DISAPPEAR)
      {
        assert(0);                /* groups cannot contain two parameters with same names */
        return FALSE;
      }

      if (param_ptr->type != LV2DYNPARAM_PARAMETER_TYPE_BOOLEAN)
      {
        /* There is pending disappear of parameter with same name but of different type */
        break;
      }

      param_ptr->data.boolean = value;
      param_ptr->plugin_callback.boolean = callback;
      param_ptr->plugin_callback_context = callback_context;
      param_ptr->pending = LV2DYNPARAM_PENDING_CHANGE;

      *param_handle_ptr = (lv2dynparam_parameter_handle)param_ptr;

      return TRUE;
    }
  }

  param_ptr = lv2dynparam_memory_pool_allocate(instance_ptr->parameters_pool);
  if (param_ptr == NULL)
  {
    return FALSE;
  }

  lv2dynparam_hints_init_empty(&param_ptr->hints);

  param_ptr->type = LV2DYNPARAM_PARAMETER_TYPE_BOOLEAN;

  memcpy(param_ptr->name, name, name_size);

  param_ptr->group_ptr = group_ptr;
  param_ptr->data.boolean = value;
  param_ptr->plugin_callback.boolean = callback;
  param_ptr->plugin_callback_context = callback_context;

  param_ptr->pending = LV2DYNPARAM_PENDING_APPEAR;
  instance_ptr->pending++;

  list_add_tail(&param_ptr->siblings, &group_ptr->child_parameters);

  lv2dynparam_plugin_param_notify(instance_ptr, param_ptr);

  *param_handle_ptr = (lv2dynparam_parameter_handle)param_ptr;

  return TRUE;
}

BOOL
lv2dynparam_plugin_param_float_add(
  lv2dynparam_plugin_instance instance_handle,
  lv2dynparam_plugin_group group,
  const char * name,
  float value,
  float min,
  float max,
  lv2dynparam_plugin_param_float_changed callback,
  void * callback_context,
  lv2dynparam_plugin_parameter * param_handle_ptr)
{
  struct lv2dynparam_plugin_parameter * param_ptr;
  struct lv2dynparam_plugin_group * group_ptr;
  struct list_head * node_ptr;
  size_t name_size;

  LOG_DEBUG("lv2dynparam_plugin_param_float_add() called for \"%s\"", name);

  name_size = strlen(name) + 1;
  if (name_size >= LV2DYNPARAM_MAX_STRING_SIZE)
  {
    assert(0);
    return FALSE;
  }

  if (group == NULL)
  {
    group_ptr = &instance_ptr->root_group;
  }
  else
  {
    group_ptr = (struct lv2dynparam_plugin_group *)group;
  }

  /* Search for same parameter in pending disappear state, and try to reuse it */
  list_for_each(node_ptr, &group_ptr->child_parameters)
  {
    param_ptr = list_entry(node_ptr, struct lv2dynparam_plugin_parameter, siblings);

    assert(param_ptr->group_ptr == group_ptr);

    if (strcmp(param_ptr->name, name) == 0)
    {
      if (param_ptr->pending != LV2DYNPARAM_PENDING_DISAPPEAR)
      {
        assert(0);                /* groups cannot contain two parameters with same names */
        return FALSE;
      }

      if (param_ptr->type != LV2DYNPARAM_PARAMETER_TYPE_BOOLEAN)
      {
        /* There is pending disappear of parameter with same name but of different type */
        break;
      }

      param_ptr->data.fpoint.value = value;
      param_ptr->data.fpoint.min = min;
      param_ptr->data.fpoint.max = max;
      param_ptr->plugin_callback.fpoint = callback;
      param_ptr->plugin_callback_context = callback_context;
      param_ptr->pending = LV2DYNPARAM_PENDING_CHANGE;

      *param_handle_ptr = (lv2dynparam_parameter_handle)param_ptr;

      return TRUE;
    }
  }

  param_ptr = lv2dynparam_memory_pool_allocate(instance_ptr->parameters_pool);
  if (param_ptr == NULL)
  {
    return FALSE;
  }

  lv2dynparam_hints_init_empty(&param_ptr->hints);

  param_ptr->type = LV2DYNPARAM_PARAMETER_TYPE_FLOAT;

  memcpy(param_ptr->name, name, name_size);

  param_ptr->group_ptr = group_ptr;
  param_ptr->data.fpoint.value = value;
  param_ptr->data.fpoint.min = min;
  param_ptr->data.fpoint.max = max;
  param_ptr->plugin_callback.fpoint = callback;
  param_ptr->plugin_callback_context = callback_context;

  param_ptr->pending = LV2DYNPARAM_PENDING_APPEAR;
  instance_ptr->pending++;

  list_add_tail(&param_ptr->siblings, &group_ptr->child_parameters);

  lv2dynparam_plugin_param_notify(instance_ptr, param_ptr);

  *param_handle_ptr = (lv2dynparam_parameter_handle)param_ptr;

  return TRUE;
}

BOOL
lv2dynparam_plugin_param_enum_add(
  lv2dynparam_plugin_instance instance_handle,
  lv2dynparam_plugin_group group,
  const char * name,
  const char ** values_ptr_ptr,
  unsigned int values_count,
  unsigned int initial_value_index,
  lv2dynparam_plugin_param_enum_changed callback,
  void * callback_context,
  lv2dynparam_plugin_parameter * param_handle_ptr)
{
  struct lv2dynparam_plugin_parameter * param_ptr;
  struct lv2dynparam_plugin_group * group_ptr;
  struct list_head * node_ptr;
  size_t name_size;
  unsigned int i;
  char ** values;

  LOG_DEBUG("lv2dynparam_plugin_param_enum_add() called for \"%s\"", name);

  for (i = 0 ; i < values_count ; i++)
  {
    LOG_DEBUG("[%u] possible value \"%s\"%s", i, values_ptr_ptr[i], i == initial_value_index ? " SELECTED" : "");
  }

  name_size = strlen(name) + 1;
  if (name_size >= LV2DYNPARAM_MAX_STRING_SIZE)
  {
    assert(0);
    goto fail;
  }

  values = lv2dynparam_enum_duplicate(instance_ptr->memory, values_ptr_ptr, values_count);
  if (values == NULL)
  {
    LOG_DEBUG("Failed to duplicate enum array");
    goto fail;
  }

  if (group == NULL)
  {
    group_ptr = &instance_ptr->root_group;
  }
  else
  {
    group_ptr = (struct lv2dynparam_plugin_group *)group;
  }

  /* Search for same parameter in pending disappear state, and try to reuse it */
  list_for_each(node_ptr, &group_ptr->child_parameters)
  {
    param_ptr = list_entry(node_ptr, struct lv2dynparam_plugin_parameter, siblings);

    assert(param_ptr->group_ptr == group_ptr);

    if (strcmp(param_ptr->name, name) == 0)
    {
      if (param_ptr->pending != LV2DYNPARAM_PENDING_DISAPPEAR)
      {
        assert(0);                /* groups cannot contain two parameters with same names */
        goto fail_free_values;
      }

      if (param_ptr->type != LV2DYNPARAM_PARAMETER_TYPE_ENUM)
      {
        /* There is pending disappear of parameter with same name but of different type */
        break;
      }

      /* free all values array.. */

      for (i = 0 ; i < param_ptr->data.enumeration.values_count ; i++)
      {
        lv2dynparam_memory_deallocate(param_ptr->data.enumeration.values[i]);
      }

      lv2dynparam_memory_deallocate(param_ptr->data.enumeration.values);

      /* update parameter data... */
      param_ptr->data.enumeration.values = values;
      param_ptr->data.enumeration.values_count = values_count;
      param_ptr->data.enumeration.selected_value = initial_value_index;

      param_ptr->plugin_callback.enumeration = callback;
      param_ptr->plugin_callback_context = callback_context;
      param_ptr->pending = LV2DYNPARAM_PENDING_CHANGE;

      *param_handle_ptr = (lv2dynparam_parameter_handle)param_ptr;

      return TRUE;
    }
  }

  param_ptr = lv2dynparam_memory_pool_allocate(instance_ptr->parameters_pool);
  if (param_ptr == NULL)
  {
    goto fail_free_values;
  }

  lv2dynparam_hints_init_empty(&param_ptr->hints);

  param_ptr->type = LV2DYNPARAM_PARAMETER_TYPE_ENUM;

  memcpy(param_ptr->name, name, name_size);

  param_ptr->group_ptr = group_ptr;

  param_ptr->data.enumeration.values = values;
  param_ptr->data.enumeration.values_count = values_count;
  param_ptr->data.enumeration.selected_value = initial_value_index;

  param_ptr->plugin_callback.enumeration = callback;
  param_ptr->plugin_callback_context = callback_context;

  param_ptr->pending = LV2DYNPARAM_PENDING_APPEAR;
  instance_ptr->pending++;

  list_add_tail(&param_ptr->siblings, &group_ptr->child_parameters);

  lv2dynparam_plugin_param_notify(instance_ptr, param_ptr);

  *param_handle_ptr = (lv2dynparam_parameter_handle)param_ptr;

  return TRUE;

fail_free_values:
  for (i = 0 ; i < values_count ; i++)
  {
    lv2dynparam_memory_deallocate(values[i]);
  }

  lv2dynparam_memory_deallocate(values);

fail:
  return FALSE;
}

BOOL
lv2dynparam_plugin_param_remove(
  lv2dynparam_plugin_instance instance_handle,
  lv2dynparam_plugin_parameter parameter)
{
  /* If in pending appear - delete it right now */
  if (parameter_ptr->pending == LV2DYNPARAM_PENDING_APPEAR)
  {
    instance_ptr->pending--;
    list_del(&parameter_ptr->siblings);
    lv2dynparam_plugin_parameter_free(instance_ptr, parameter_ptr);
    return TRUE;
  }

  parameter_ptr->pending = LV2DYNPARAM_PENDING_DISAPPEAR;
  instance_ptr->pending++;
  lv2dynparam_plugin_param_notify(instance_ptr, parameter_ptr);

  return TRUE;
}
