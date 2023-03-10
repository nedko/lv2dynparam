/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of lv2dynparam plugin library
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
#include <assert.h>
#include <stdbool.h>
#include <lv2.h>

#include "../lv2dynparam.h"
#include "../list.h"
#include "plugin.h"
#include "../memory_atomic.h"
#include "../hint_set.h"
#include "internal.h"
#define LOG_LEVEL LOG_LEVEL_ERROR
#include "../log.h"

bool
lv2dynparam_plugin_group_init(
  struct lv2dynparam_plugin_instance * instance_ptr,
  struct lv2dynparam_plugin_group * group_ptr,
  struct lv2dynparam_plugin_group * parent_group_ptr,
  const struct lv2dynparam_hints * hints_ptr,
  const char * name)
{
  size_t name_size;
  unsigned int index;

  LOG_DEBUG("lv2dynparam_plugin_group_init() called for \"%s\"", name);

  name_size = strlen(name) + 1;
  if (name_size >= LV2DYNPARAM_MAX_STRING_SIZE)
  {
    assert(0);
    return false;
  }

  if (hints_ptr != NULL)
  {
    LOG_DEBUG("%u hints", hints_ptr->count);
    for (index = 0 ; index < hints_ptr->count ; index++)
    {
      if (hints_ptr->values[index] == NULL)
      {
        LOG_DEBUG("  \"%s\"", hints_ptr->names[index]);
      }
      else
      {
        LOG_DEBUG("  \"%s\":\"%s\"", hints_ptr->names[index], hints_ptr->values[index]);
      }
    }

    if (!lv2dynparam_hints_init_copy(instance_ptr->memory, hints_ptr, &group_ptr->hints))
    {
      return false;
    }
  }
  else
  {
    lv2dynparam_hints_init_empty(&group_ptr->hints);
  }

  memcpy(group_ptr->name, name, name_size);
  group_ptr->group_ptr = parent_group_ptr;
  INIT_LIST_HEAD(&group_ptr->child_groups);
  INIT_LIST_HEAD(&group_ptr->child_parameters);

  group_ptr->pending = LV2DYNPARAM_PENDING_APPEAR;
  instance_ptr->pending++;

  return true;
}

bool
lv2dynparam_plugin_group_new(
  struct lv2dynparam_plugin_instance * instance_ptr,
  struct lv2dynparam_plugin_group * parent_group_ptr,
  const char * name,
  const struct lv2dynparam_hints * hints_ptr,
  struct lv2dynparam_plugin_group ** group_ptr_ptr)
{
  bool ret;
  struct lv2dynparam_plugin_group * group_ptr;

  group_ptr = rtsafe_memory_pool_allocate(instance_ptr->groups_pool);
  if (group_ptr == NULL)
  {
    ret = false;
    LOG_DEBUG("rtsafe_memory_pool_allocate() failed");
    goto exit;
  }

  if (!lv2dynparam_plugin_group_init(instance_ptr, group_ptr, parent_group_ptr, hints_ptr, name))
  {
    ret = false;
    LOG_DEBUG("lv2dynparam_plugin_group_init() failed");
    goto free;
  }

  list_add_tail(&group_ptr->siblings, &parent_group_ptr->child_groups);

  *group_ptr_ptr = group_ptr;

  return true;

free:
  rtsafe_memory_pool_deallocate(instance_ptr->groups_pool, group_ptr);

exit:
  return ret;
}

void
lv2dynparam_plugin_group_clean(
  struct lv2dynparam_plugin_instance * instance_ptr,
  struct lv2dynparam_plugin_group * group_ptr)
{
  struct list_head * node_ptr;
  struct lv2dynparam_plugin_group * child_group_ptr;
  struct lv2dynparam_plugin_parameter * child_param_ptr;

  LOG_DEBUG("Cleaning group \"%s\"", group_ptr->name);

  while (!list_empty(&group_ptr->child_groups))
  {
    node_ptr = group_ptr->child_groups.next;
    child_group_ptr = list_entry(node_ptr, struct lv2dynparam_plugin_group, siblings);
    list_del(node_ptr);
    lv2dynparam_plugin_group_free(instance_ptr, child_group_ptr);
  }

  LOG_DEBUG("Cleaning parameters...");

  while (!list_empty(&group_ptr->child_parameters))
  {
    node_ptr = group_ptr->child_parameters.next;
    child_param_ptr = list_entry(node_ptr, struct lv2dynparam_plugin_parameter, siblings);
    list_del(node_ptr);
    lv2dynparam_plugin_parameter_free(instance_ptr, child_param_ptr);
  }

  lv2dynparam_hints_clear(&group_ptr->hints);
}

void
lv2dynparam_plugin_group_free(
  struct lv2dynparam_plugin_instance * instance_ptr,
  struct lv2dynparam_plugin_group * group_ptr)
{
  lv2dynparam_plugin_group_clean(instance_ptr, group_ptr);
  LOG_DEBUG("Freeing group \"%s\"", group_ptr->name);
  rtsafe_memory_pool_deallocate(instance_ptr->groups_pool, group_ptr);
}

void
lv2dynparam_plugin_group_notify(
  struct lv2dynparam_plugin_instance * instance_ptr,
  struct lv2dynparam_plugin_group * group_ptr)
{
  struct list_head * node_ptr;
  struct lv2dynparam_plugin_group * child_group_ptr;
  struct lv2dynparam_plugin_parameter * child_param_ptr;

  if (instance_ptr->host_callbacks == NULL)
  {
    /* Host not attached */
    return;
  }

  switch (group_ptr->pending)
  {
  case LV2DYNPARAM_PENDING_NOTHING:
    /* There is nothing to notify for */
    return;
  case LV2DYNPARAM_PENDING_APPEAR:
    if (instance_ptr->host_callbacks->group_appear(
          instance_ptr->host_context,
          group_ptr->group_ptr == NULL ? NULL : group_ptr->group_ptr->host_context, /* host context of parent group */
          (lv2dynparam_group_handle)group_ptr,
          &group_ptr->hints,
          &group_ptr->host_context))
    {
      group_ptr->pending = LV2DYNPARAM_PENDING_NOTHING;
      instance_ptr->pending--;
    }

    list_for_each(node_ptr, &group_ptr->child_groups)
    {
      child_group_ptr = list_entry(node_ptr, struct lv2dynparam_plugin_group, siblings);

      assert(child_group_ptr->group_ptr == group_ptr);

      lv2dynparam_plugin_group_notify(instance_ptr, child_group_ptr);

/*       if (instance_ptr->pending == 0) */
/*       { */
        /* optimization */
/*         return; */
/*       } */
    }

    list_for_each(node_ptr, &group_ptr->child_parameters)
    {
      child_param_ptr = list_entry(node_ptr, struct lv2dynparam_plugin_parameter, siblings);

      assert(child_param_ptr->group_ptr == group_ptr);

      lv2dynparam_plugin_param_notify(instance_ptr, child_param_ptr);

/*       if (instance_ptr->pending == 0) */
/*       { */
        /* optimization */
/*         return; */
/*       } */
    }
    return;
  default:
    assert(0);
  }
}

#define group_ptr ((struct lv2dynparam_plugin_group *)group)

void
lv2dynparam_plugin_group_get_name(
  lv2dynparam_group_handle group,
  char * buffer)
{
  size_t s;

  s = strlen(group_ptr->name) + 1;

  s++;

  assert(s <= LV2DYNPARAM_MAX_STRING_SIZE);

  memcpy(buffer, group_ptr->name, s);
}

#undef group_ptr
#define parent_group_ptr ((struct lv2dynparam_plugin_group *)parent_group)
#define instance_ptr ((struct lv2dynparam_plugin_instance *)instance_handle)

bool
lv2dynparam_plugin_group_add(
  lv2dynparam_plugin_instance instance_handle,
  lv2dynparam_plugin_group parent_group,
  const char * name,
  const struct lv2dynparam_hints * hints_ptr,
  lv2dynparam_plugin_group * group_handle_ptr)
{
  struct lv2dynparam_plugin_group * group_ptr;

  LOG_DEBUG("lv2dynparam_plugin_group_add() for \"%s\" enter.", name);

  if (!lv2dynparam_plugin_group_new(
        instance_ptr,
        parent_group_ptr == NULL ? &instance_ptr->root_group: parent_group_ptr,
        name,
        hints_ptr,
        &group_ptr))
  {
    return false;
  }

  lv2dynparam_plugin_group_notify(instance_ptr, group_ptr);

  *group_handle_ptr = (lv2dynparam_plugin_group)group_ptr;

  LOG_DEBUG("lv2dynparam_plugin_group_add() for \"%s\" leave", name);

  return true;
}
