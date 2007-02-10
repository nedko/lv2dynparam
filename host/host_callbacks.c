/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   Implementations of lv2dynparam host callbacks, the ones called by plugin
 *
 *   This file is part of lv2dynparam host library
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
#include <assert.h>
#include <string.h>

#include "../lv2.h"
#include "../lv2dynparam.h"
#include "host.h"
#include "../audiolock.h"
#include "../list.h"
#include "../memory_atomic.h"
#include "internal.h"
#include "host_callbacks.h"
#include "../helpers.h"

//#define LOG_LEVEL LOG_LEVEL_DEBUG
#include "../log.h"

#define instance_ptr ((struct lv2dynparam_host_instance *)instance_host_context)

unsigned char
lv2dynparam_host_group_appear(
  void * instance_host_context,
  void * parent_group_host_context,
  lv2dynparam_group_handle group,
  void ** group_host_context)
{
  struct lv2dynparam_host_group * group_ptr;
  struct lv2dynparam_host_group * parent_group_ptr;

  parent_group_ptr = (struct lv2dynparam_host_group *)parent_group_host_context;

  if (parent_group_ptr == NULL &&
      instance_ptr->root_group_ptr != NULL)
  {
    /* Top level groups cannot disappear */
    LOG_ERROR("Plugin tried to create two top level groups");
    goto fail;
  }

  group_ptr = lv2dynparam_memory_pool_allocate(instance_ptr->groups_pool);
  if (group_ptr == NULL)
  {
    /* we are not lucky enough, plugin will retry later */
    goto fail;
  }

  group_ptr->parent_group_ptr = parent_group_ptr;

  group_ptr->group_handle = group;

  INIT_LIST_HEAD(&group_ptr->child_groups);
  INIT_LIST_HEAD(&group_ptr->child_params);
  INIT_LIST_HEAD(&group_ptr->child_commands);

  instance_ptr->callbacks_ptr->group_get_name(group, group_ptr->name);
  instance_ptr->callbacks_ptr->group_get_type_uri(group, group_ptr->type_uri);

  group_ptr->pending_state = LV2DYNPARAM_PENDING_APPEAR;
  group_ptr->pending_childern_count = 0;

  if (parent_group_ptr == NULL)
  {
    LOG_DEBUG("The top level group \"%s\" of type \"%s\" appeared", group_ptr->name, group_ptr->type_uri);
    instance_ptr->root_group_ptr = group_ptr;
  }
  else
  {
    LOG_DEBUG("Group \"%s\" of type \"%s\" with parent \"%s\" appeared.", group_ptr->name, group_ptr->type_uri, parent_group_ptr->name);
    list_add_tail(&group_ptr->siblings, &parent_group_ptr->child_groups);

    lv2dynparam_host_group_pending_children_count_increment(parent_group_ptr);
  }

  *group_host_context = group_ptr;

  return TRUE;

fail:
  return FALSE;
}

unsigned char
lv2dynparam_host_group_disappear(
  void * instance_host_context,
  void * group_host_context)
{
  return TRUE;
}

unsigned char
lv2dynparam_host_parameter_appear(
  void * instance_host_context,
  void * group_host_context,
  lv2dynparam_parameter_handle parameter,
  void ** parameter_host_context)
{
  struct lv2dynparam_host_parameter * param_ptr;
  struct lv2dynparam_host_group * group_ptr;
  unsigned int i;

  group_ptr = (struct lv2dynparam_host_group *)group_host_context;

  param_ptr = lv2dynparam_memory_pool_allocate(instance_ptr->parameters_pool);
  if (param_ptr == NULL)
  {
    return FALSE;
  }

  param_ptr->param_handle = parameter;

  instance_ptr->callbacks_ptr->parameter_get_name(parameter, param_ptr->name);
  instance_ptr->callbacks_ptr->parameter_get_type_uri(parameter, param_ptr->type_uri);

  LOG_DEBUG("Parameter \"%s\" of type \"%s\" with parent \"%s\" appeared.", param_ptr->name, param_ptr->type_uri, group_ptr->name);

  if (!lv2dynparam_host_map_type_uri(param_ptr))
  {
    LOG_WARNING("Ignoring parameter of unknown type \"%s\"", param_ptr->type_uri);

    lv2dynparam_memory_pool_deallocate(instance_ptr->parameters_pool, param_ptr);

    *parameter_host_context = NULL;

    return TRUE;
  }

  instance_ptr->callbacks_ptr->parameter_get_value(
    parameter,
    &param_ptr->value_ptr);

  if (param_ptr->type == LV2DYNPARAM_PARAMETER_TYPE_FLOAT ||
      param_ptr->type == LV2DYNPARAM_PARAMETER_TYPE_INT ||
      param_ptr->type == LV2DYNPARAM_PARAMETER_TYPE_NOTE ||
      param_ptr->type == LV2DYNPARAM_PARAMETER_TYPE_ENUM)
  {
    /* ranged type */
    instance_ptr->callbacks_ptr->parameter_get_range(
      parameter,
      &param_ptr->min_ptr,
      &param_ptr->max_ptr);
  }
  else
  {
    /* unranged type, range predefined or not applicable */
    param_ptr->min_ptr = NULL;
    param_ptr->max_ptr = NULL;
  }

  /* read current value */
  switch (param_ptr->type)
  {
  case LV2DYNPARAM_PARAMETER_TYPE_BOOLEAN:
    param_ptr->data.boolean = *(unsigned char *)(param_ptr->value_ptr);
    LOG_DEBUG("Boolean parameter with value %s", param_ptr->data.boolean ? "TRUE" : "FALSE");
    break;
  case LV2DYNPARAM_PARAMETER_TYPE_FLOAT:
    param_ptr->data.fpoint.value = *(float *)(param_ptr->value_ptr);
    LOG_DEBUG("Float parameter with value %f", param_ptr->data.fpoint.value);
    break;
  case LV2DYNPARAM_PARAMETER_TYPE_ENUM:
    param_ptr->data.enumeration.selected_value = *(unsigned int *)(param_ptr->value_ptr);
    LOG_DEBUG("Enum parameter with selected value index %u", param_ptr->data.enumeration.selected_value);
    break;
  }

  /* read current range */
  switch (param_ptr->type)
  {
  case LV2DYNPARAM_PARAMETER_TYPE_FLOAT:
    param_ptr->data.fpoint.min = *(float *)(param_ptr->min_ptr);
    param_ptr->data.fpoint.max = *(float *)(param_ptr->max_ptr);
    LOG_DEBUG("Float parameter with range %f - %f", param_ptr->data.fpoint.min, param_ptr->data.fpoint.max);
    break;
  case LV2DYNPARAM_PARAMETER_TYPE_ENUM:
    param_ptr->data.enumeration.values_count = *(unsigned int *)(param_ptr->max_ptr);

/*     LOG_DEBUG("Enum parameter with %u possible values", param_ptr->data.enumeration.values_count); */
/*     for (i = 0 ; i < param_ptr->data.enumeration.values_count ; i++) */
/*     { */
/*       LOG_DEBUG("\"%s\"", (*(const char * const * *)(param_ptr->min_ptr))[i]); */
/*     } */

    param_ptr->data.enumeration.values =
      lv2dynparam_enum_duplicate(
        instance_ptr->memory,
        *(const char * const * *)(param_ptr->min_ptr),
        param_ptr->data.enumeration.values_count);
    if (param_ptr->data.enumeration.values == NULL)
    {
      lv2dynparam_memory_pool_deallocate(instance_ptr->parameters_pool, param_ptr);
      return FALSE;
    }

    LOG_DEBUG("Enum parameter with %u possible values", param_ptr->data.enumeration.values_count);
    for (i = 0 ; i < param_ptr->data.enumeration.values_count ; i++)
    {
      LOG_DEBUG("\"%s\"", param_ptr->data.enumeration.values[i]);
    }

    LOG_DEBUG(
      "Selected value is \"%s\" at index  %u",
      param_ptr->data.enumeration.values[param_ptr->data.enumeration.selected_value],
      param_ptr->data.enumeration.selected_value);

    break;
  }

  /* Add parameter as child of its group */
  param_ptr->group_ptr = group_ptr;
  list_add_tail(&param_ptr->siblings, &group_ptr->child_params);
  param_ptr->pending_state = LV2DYNPARAM_PENDING_APPEAR;
  lv2dynparam_host_group_pending_children_count_increment(group_ptr);

  *parameter_host_context = param_ptr;

  return TRUE;
}

#define param_ptr ((struct lv2dynparam_host_parameter *)parameter_host_context)

unsigned char
lv2dynparam_host_parameter_disappear(
  void * instance_host_context,
  void * parameter_host_context)
{
  if (param_ptr == NULL)
  {
    LOG_DEBUG("Ignored parameter disappeared.");
  }

  LOG_DEBUG("Parameter %s disappeared.", param_ptr->name);

  param_ptr->pending_state = LV2DYNPARAM_PENDING_DISAPPEAR;
  lv2dynparam_host_group_pending_children_count_increment(param_ptr->group_ptr);

  return TRUE;
}

unsigned char
lv2dynparam_host_parameter_change(
  void * instance_host_context,
  void * parameter_host_context)
{
  return TRUE;
}

unsigned char
lv2dynparam_host_command_appear(
  void * instance_host_context,
  void * group_host_context,
  lv2dynparam_command_handle command,
  void ** command_context)
{
  LOG_DEBUG("Command apperead.");
  return FALSE;                 /* not implemented */
}

unsigned char
lv2dynparam_host_command_disappear(
  void * instance_host_context,
  void * command_host_context)
{
  return TRUE;
}
