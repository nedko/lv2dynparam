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
#include "plugin.h"
#include "../list.h"
#include "../memory_atomic.h"
#include "internal.h"
//#define LOG_LEVEL LOG_LEVEL_DEBUG
#include "../log.h"

static struct lv2dynparam_plugin_callbacks g_lv2dynparam_plugin_callbacks =
{
  .host_attach = lv2dynparam_plugin_host_attach,

  .group_get_name = lv2dynparam_plugin_group_get_name,

  .parameter_get_type_uri = lv2dynparam_plugin_parameter_get_type_uri,
  .parameter_get_name = lv2dynparam_plugin_parameter_get_name,
  .parameter_get_value = lv2dynparam_plugin_parameter_get_value,
  .parameter_get_range = lv2dynparam_plugin_parameter_get_range,
  .parameter_change = lv2dynparam_plugin_parameter_change
};

static struct list_head g_instances;

void lv2dynparam_plugin_initialise() __attribute__((constructor));
void lv2dynparam_plugin_initialise()
{
  LOG_DEBUG("lv2dynparam_plugin_initialise() called");
  INIT_LIST_HEAD(&g_instances);
}

void *
get_lv2dynparam_plugin_extension_data(void)
{
  return &g_lv2dynparam_plugin_callbacks;
}

BOOL
lv2dynparam_plugin_instantiate(
  LV2_Handle lv2instance,
  const char * root_group_name,
  lv2dynparam_plugin_instance * instance_handle_ptr)
{
  BOOL ret;
  struct lv2dynparam_plugin_instance * instance_ptr;

  instance_ptr = malloc(sizeof(struct lv2dynparam_plugin_instance));
  if (instance_ptr == NULL)
  {
    ret = FALSE;
    goto exit;
  }

  if (!lv2dynparam_memory_init(
        64 * 1024,
        100,
        1000,
        &instance_ptr->memory))
  {
    ret = FALSE;
    goto free_instance;
  }

  if (!lv2dynparam_memory_pool_create(
        sizeof(struct lv2dynparam_plugin_group),
        100,
        1000,
        &instance_ptr->groups_pool))
  {
    ret = FALSE;
    goto free_uninit_memory;
  }

  if (!lv2dynparam_memory_pool_create(
        sizeof(struct lv2dynparam_plugin_parameter),
        100,
        1000,
        &instance_ptr->parameters_pool))
  {
    ret = FALSE;
    goto free_destroy_groups_pool;
  }

  instance_ptr->lv2instance = lv2instance;

  if (!lv2dynparam_plugin_group_init(
        instance_ptr,
        &instance_ptr->root_group,
        NULL,
        NULL,
        root_group_name))
  {
    ret = FALSE;
    goto free_destroy_parameters_pool;
  }

  list_add_tail(&instance_ptr->siblings, &g_instances);

  instance_ptr->host_callbacks = NULL;

  instance_ptr->pending = 0;

  *instance_handle_ptr = instance_ptr;

  ret = TRUE;
  goto exit;

free_destroy_parameters_pool:

free_destroy_groups_pool:

free_uninit_memory:

free_instance:
  free(instance_ptr);

exit:
  return ret;
}

unsigned char
lv2dynparam_plugin_host_attach(
  LV2_Handle instance,
  struct lv2dynparam_host_callbacks * host_callbacks,
  void * instance_host_context)
{
  struct lv2dynparam_plugin_instance * instance_ptr;
  struct list_head * node_ptr;

  list_for_each(node_ptr, &g_instances)
  {
    instance_ptr = list_entry(node_ptr, struct lv2dynparam_plugin_instance, siblings);
    if (instance_ptr->lv2instance == instance)
    {
      goto instance_found;
    }
  }

  return FALSE;

instance_found:
  instance_ptr->host_callbacks = host_callbacks;
  instance_ptr->host_context = instance_host_context;

  //LOG_DEBUG("lv2dynparam_plugin_host_attach(): instance_ptr->pending is %u", instance_ptr->pending);
  //if (instance_ptr->pending != 0) /* optimization */
  {
    lv2dynparam_plugin_group_notify(instance_ptr, &instance_ptr->root_group);
  }

  return TRUE;
}

#define instance_ptr ((struct lv2dynparam_plugin_instance *)instance_handle)

void
lv2dynparam_plugin_cleanup(
  lv2dynparam_plugin_instance instance_handle)
{
  list_del(&instance_ptr->siblings);
  lv2dynparam_plugin_group_clean(instance_ptr, &instance_ptr->root_group);
  lv2dynparam_memory_pool_destroy(instance_ptr->parameters_pool);
  lv2dynparam_memory_pool_destroy(instance_ptr->groups_pool);
  lv2dynparam_memory_uninit(instance_ptr->memory);
  free(instance_ptr);
}
