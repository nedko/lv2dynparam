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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../lv2.h"
#include "../lv2dynparam.h"
#include "../list.h"
#include "plugin.h"
#include "dynparam_internal.h"
#define LOG_LEVEL LOG_LEVEL_ERROR
#include "../log.h"

BOOL
lv2dynparam_plugin_group_init(
  struct lv2dynparam_plugin_instance * instance_ptr,
  struct lv2dynparam_plugin_group * group_ptr,
  struct lv2dynparam_plugin_group * parent_group_ptr,
  const char * name,
  const char * type_uri)
{
  size_t name_size;
  size_t type_uri_size;

  LOG_DEBUG("lv2dynparam_plugin_group_init() called for \"%s\"", name);

  name_size = strlen(name) + 1;
  if (name_size >= LV2DYNPARAM_MAX_STRING_SIZE)
  {
    assert(0);
    return FALSE;
  }

  type_uri_size = strlen(type_uri) + 1;
  if (type_uri_size >= LV2DYNPARAM_MAX_STRING_SIZE)
  {
    assert(0);
    return FALSE;
  }

  memcpy(group_ptr->name, name, name_size);
  memcpy(group_ptr->type_uri, type_uri, type_uri_size);
  group_ptr->group_ptr = parent_group_ptr;
  INIT_LIST_HEAD(&group_ptr->child_groups);
  INIT_LIST_HEAD(&group_ptr->child_parameters);

  group_ptr->pending = LV2DYNPARAM_PENDING_APPEAR;
  instance_ptr->pending++;

  return TRUE;
}

BOOL
lv2dynparam_plugin_group_new(
  struct lv2dynparam_plugin_instance * instance_ptr,
  struct lv2dynparam_plugin_group * parent_group_ptr,
  const char * name,
  const char * type_uri,
  struct lv2dynparam_plugin_group ** group_ptr_ptr)
{
  BOOL ret;
  struct lv2dynparam_plugin_group * group_ptr;

  /* FIXME: don't sleep */
  group_ptr = malloc(sizeof(struct lv2dynparam_plugin_group));
  if (group_ptr == NULL)
  {
    ret = FALSE;
    goto exit;
  }

  if (!lv2dynparam_plugin_group_init(instance_ptr, group_ptr, parent_group_ptr, name, type_uri))
  {
    ret = FALSE;
    goto free;
  }

  list_add_tail(&group_ptr->siblings, &parent_group_ptr->child_groups);

  *group_ptr_ptr = group_ptr;

  return TRUE;

free:
  free(group_ptr);

exit:
  return ret;
}

void
lv2dynparam_plugin_group_clean(
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
    lv2dynparam_plugin_group_free(child_group_ptr);
  }

  LOG_DEBUG("Cleaning parameters...");

  while (!list_empty(&group_ptr->child_parameters))
  {
    node_ptr = group_ptr->child_parameters.next;
    child_param_ptr = list_entry(node_ptr, struct lv2dynparam_plugin_parameter, siblings);
    list_del(node_ptr);
    lv2dynparam_plugin_parameter_free(child_param_ptr);
  }
}

void
lv2dynparam_plugin_group_free(
  struct lv2dynparam_plugin_group * group_ptr)
{
  lv2dynparam_plugin_group_clean(group_ptr);
  LOG_DEBUG("Freeing group \"%s\"", group_ptr->name);
  free(group_ptr);
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
lv2dynparam_plugin_group_get_type_uri(
  lv2dynparam_group_handle group,
  char * buffer)
{
  size_t s;

  s = strlen(group_ptr->type_uri) + 1;

  assert(s <= LV2DYNPARAM_MAX_STRING_SIZE);

  memcpy(buffer, group_ptr->type_uri, s);
}

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

BOOL
lv2dynparam_plugin_group_add(
  lv2dynparam_plugin_instance instance_handle,
  lv2dynparam_plugin_group parent_group,
  const char * name,
  const char * type_uri,
  lv2dynparam_plugin_group * group_handle_ptr)
{
  struct lv2dynparam_plugin_group * group_ptr;

  LOG_DEBUG("lv2dynparam_plugin_group_add() for \"%s\" enter.", name);

  if (!lv2dynparam_plugin_group_new(
        instance_ptr,
        parent_group_ptr == NULL ? &instance_ptr->root_group: parent_group_ptr,
        name,
        type_uri,
        &group_ptr))
  {
    return FALSE;
  }

  lv2dynparam_plugin_group_notify(instance_ptr, group_ptr);

  *group_handle_ptr = (lv2dynparam_plugin_group)group_ptr;

  LOG_DEBUG("lv2dynparam_plugin_group_add() for \"%s\" leave", name);

  return TRUE;
}
