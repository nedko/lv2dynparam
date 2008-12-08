/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
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
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <locale.h>
#include <lv2.h>

#include "../lv2dynparam.h"
#include "../lv2_rtmempool.h"
#include "host.h"
#include "../audiolock.h"
#include "../list.h"
#include "../memory_atomic.h"
#include "internal.h"
#include "host_callbacks.h"
#include "../helpers.h"
#include "../hint_set.h"

#define LOG_LEVEL LOG_LEVEL_ERROR
#include "../log.h"

#define SERIALIZE_SEPARATOR_CHAR          '/'
#define SERIALIZE_ESCAPE_CHAR             '^'
#define SERIALIZE_BUFFER_INITIAL_SIZE     1024
#define SERIALIZE_BUFFER_INCREMENT_SIZE   1024

#define SERIALIZE_TYPE_CHAR_BOOLEAN 'b'
#define SERIALIZE_TYPE_CHAR_FLOAT   'f'
#define SERIALIZE_TYPE_CHAR_INT     'i'
#define SERIALIZE_TYPE_CHAR_STRING  's'

void
lv2dynparam_host_parameter_free(
  struct lv2dynparam_host_instance * instance_ptr,
  struct lv2dynparam_host_parameter * parameter_ptr)
{
  switch (parameter_ptr->type)
  {
  case LV2DYNPARAM_PARAMETER_TYPE_ENUM:
    lv2dynparam_enum_free(
      instance_ptr->memory,
      parameter_ptr->data.enumeration.values,
      parameter_ptr->data.enumeration.values_count);
    return;
  }

  lv2dynparam_hints_clear(&parameter_ptr->hints);
  rtsafe_memory_pool_deallocate(instance_ptr->parameters_pool, parameter_ptr);
}

void
lv2dynparam_host_group_free(
  struct lv2dynparam_host_instance * instance_ptr,
  struct lv2dynparam_host_group * group_ptr)
{
  lv2dynparam_hints_clear(&group_ptr->hints);
  rtsafe_memory_pool_deallocate(instance_ptr->groups_pool, group_ptr);
}

bool
lv2dynparam_host_map_type_uri(
  struct lv2dynparam_host_parameter * parameter_ptr)
{
  if (strcmp(parameter_ptr->type_uri, LV2DYNPARAM_PARAMETER_TYPE_BOOLEAN_URI) == 0)
  {
    parameter_ptr->type = LV2DYNPARAM_PARAMETER_TYPE_BOOLEAN;
    return true;
  }

  if (strcmp(parameter_ptr->type_uri, LV2DYNPARAM_PARAMETER_TYPE_FLOAT_URI) == 0)
  {
    parameter_ptr->type = LV2DYNPARAM_PARAMETER_TYPE_FLOAT;
    return true;
  }

  if (strcmp(parameter_ptr->type_uri, LV2DYNPARAM_PARAMETER_TYPE_ENUM_URI) == 0)
  {
    parameter_ptr->type = LV2DYNPARAM_PARAMETER_TYPE_ENUM;
    return true;
  }

  if (strcmp(parameter_ptr->type_uri, LV2DYNPARAM_PARAMETER_TYPE_INT_URI) == 0)
  {
    parameter_ptr->type = LV2DYNPARAM_PARAMETER_TYPE_INT;
    return true;
  }

  return false;
}

static struct lv2dynparam_host_callbacks g_lv2dynparam_host_callbacks =
{
  .group_appear = lv2dynparam_host_group_appear,
  .group_disappear = lv2dynparam_host_group_disappear,

  .parameter_appear = lv2dynparam_host_parameter_appear,
  .parameter_disappear = lv2dynparam_host_parameter_disappear,
  .parameter_change = lv2dynparam_host_parameter_change,

  .command_appear = lv2dynparam_host_command_appear,
  .command_disappear = lv2dynparam_host_command_disappear
};

bool
lv2dynparam_host_attach(
  const LV2_Descriptor * lv2descriptor,
  LV2_Handle lv2instance,
  struct lv2_rtsafe_memory_pool_provider * rtmempool_ptr,
  void * instance_ui_context,
  lv2dynparam_host_instance * instance_handle_ptr)
{
  struct lv2dynparam_host_instance * instance_ptr;

  instance_ptr = malloc(sizeof(struct lv2dynparam_host_instance));
  if (instance_ptr == NULL)
  {
    /* out of memory */
    goto fail;
  }

  instance_ptr->instance_ui_context = instance_ui_context;

  instance_ptr->lock = audiolock_create_slow();
  if (instance_ptr->lock == AUDIOLOCK_HANDLE_INVALID_VALUE)
  {
    goto fail_free;
  }

  if (!rtsafe_memory_pool_create(
        rtmempool_ptr,
        "host groups",
        sizeof(struct lv2dynparam_host_group),
        10,
        100,
        &instance_ptr->groups_pool))
  {
    goto fail_destroy_lock;
  }

  if (!rtsafe_memory_pool_create(
        rtmempool_ptr,
        "host parameters",
        sizeof(struct lv2dynparam_host_parameter),
        10,
        100,
        &instance_ptr->parameters_pool))
  {
    goto fail_destroy_groups_pool;
  }

  if (!rtsafe_memory_pool_create(
        rtmempool_ptr,
        "host messages",
        sizeof(struct lv2dynparam_host_message),
        10,
        100,
        &instance_ptr->messages_pool))
  {
    goto fail_destroy_parameters_pool;
  }

  if (!rtsafe_memory_pool_create(
        rtmempool_ptr,
        "host pending parameter value changes",
        sizeof(struct lv2dynparam_host_parameter_pending_value_change),
        10,
        100,
        &instance_ptr->pending_parameter_value_changes_pool))
  {
    goto fail_destroy_messages_pool;
  }

  if (!rtsafe_memory_init(
        rtmempool_ptr,
        4 * 1024,
        10,
        100,
        &instance_ptr->memory))
  {
    goto fail_destroy_pending_parameter_value_changes_pool;
  }

  instance_ptr->callbacks_ptr = lv2descriptor->extension_data(LV2DYNPARAM_URI);
  if (instance_ptr->callbacks_ptr == NULL)
  {
    /* Oh my! plugin does not support dynparam extension! This should be pre-checked by caller! */
    goto fail_uninit_memory;
  }

  INIT_LIST_HEAD(&instance_ptr->realtime_to_ui_queue);
  INIT_LIST_HEAD(&instance_ptr->ui_to_realtime_queue);
  INIT_LIST_HEAD(&instance_ptr->pending_parameter_value_changes);
  instance_ptr->lv2instance = lv2instance;
  instance_ptr->root_group_ptr = NULL;
  instance_ptr->ui = false;

  if (!instance_ptr->callbacks_ptr->host_attach(
        lv2instance,
        &g_lv2dynparam_host_callbacks,
        instance_ptr))
  {
    LOG_ERROR("lv2dynparam host_attach() failed.");
    goto fail_uninit_memory;
  }

  /* switch to atomic memory mode */
  rtsafe_memory_atomic(instance_ptr->memory);
  rtsafe_memory_pool_atomic(instance_ptr->groups_pool);
  rtsafe_memory_pool_atomic(instance_ptr->parameters_pool);
  rtsafe_memory_pool_atomic(instance_ptr->messages_pool);

  *instance_handle_ptr = (lv2dynparam_host_instance)instance_ptr;

  return 1;

fail_uninit_memory:
  rtsafe_memory_uninit(instance_ptr->memory);

fail_destroy_pending_parameter_value_changes_pool:
  rtsafe_memory_pool_destroy(instance_ptr->pending_parameter_value_changes_pool);

fail_destroy_messages_pool:
  rtsafe_memory_pool_destroy(instance_ptr->messages_pool);

fail_destroy_parameters_pool:
  rtsafe_memory_pool_destroy(instance_ptr->parameters_pool);

fail_destroy_groups_pool:
  rtsafe_memory_pool_destroy(instance_ptr->groups_pool);

fail_destroy_lock:
  audiolock_destroy(instance_ptr->lock);

fail_free:
  free(instance_ptr);

fail:
  return 0;
}

void
lv2dynparam_host_group_pending_children_count_increment(
  struct lv2dynparam_host_group * group_ptr)
{
  group_ptr->pending_childern_count++;

  if (group_ptr->parent_group_ptr != NULL)
  {
    lv2dynparam_host_group_pending_children_count_increment(group_ptr->parent_group_ptr);
  }
}

void
lv2dynparam_host_group_pending_children_count_decrement(
  struct lv2dynparam_host_group * group_ptr)
{
  assert(group_ptr->pending_childern_count != 0);

  group_ptr->pending_childern_count--;

  if (group_ptr->parent_group_ptr != NULL)
  {
    lv2dynparam_host_group_pending_children_count_decrement(group_ptr->parent_group_ptr);
  }
}

void
lv2dynparam_host_notify_group_appeared(
  struct lv2dynparam_host_instance * instance_ptr,
  struct lv2dynparam_host_group * group_ptr)
{
  void * parent_group_ui_context;

  LOG_DEBUG("lv2dynparam_host_notify_group_appeared() called for '%s'.", group_ptr->name);

  if (group_ptr->parent_group_ptr)
  {
    parent_group_ui_context = group_ptr->parent_group_ptr->ui_context;
  }
  else
  {
    /* Master Yoda says: The root group it is */
    parent_group_ui_context = NULL;
  }

  dynparam_group_appeared(
    group_ptr,
    instance_ptr->instance_ui_context,
    parent_group_ui_context,
    group_ptr->name,
    &group_ptr->hints,
    &group_ptr->ui_context);
}

void
lv2dynparam_host_notify_group_disappeared(
  struct lv2dynparam_host_instance * instance_ptr,
  struct lv2dynparam_host_group * group_ptr)
{
  void * parent_group_ui_context;

  LOG_DEBUG("lv2dynparam_host_notify_group_disappeared() called for '%s'.", group_ptr->name);

  if (group_ptr->parent_group_ptr)
  {
    parent_group_ui_context = group_ptr->parent_group_ptr->ui_context;
  }
  else
  {
    /* Master Yoda says: The root group it is */
    parent_group_ui_context = NULL;
  }

  dynparam_group_disappeared(
    instance_ptr->instance_ui_context,
    parent_group_ui_context,
    group_ptr->ui_context);
}

static
const char *
get_prev_component(
  const char * begining_ptr,
  const char * char_ptr)
{
  if (char_ptr == begining_ptr)
  {
    return NULL;
  }

  char_ptr--;

loop:
  if (char_ptr == begining_ptr || char_ptr[-1] == 0)
  {
    return char_ptr;
  }

  char_ptr--;
  goto loop;
}

static
bool
parameter_asciizz_match(
  struct lv2dynparam_host_instance * instance_ptr,
  struct lv2dynparam_host_parameter * parameter_ptr,
  const char * asciizz)
{
  const char * component;
  const char * name;
  struct lv2dynparam_host_group * group_ptr;

  component = asciizz;

  do
  {
    component = component + strlen(component) + 1;
  }
  while (*component != 0);

  group_ptr = NULL;
  name = parameter_ptr->name;

loop:
  component = get_prev_component(asciizz, component);
  if (!component)
  {
    return group_ptr == instance_ptr->root_group_ptr;
  }

  if (group_ptr == instance_ptr->root_group_ptr)
  {
    return false;
  }

  //LOG_DEBUG("'%s' ?= '%s'", component, name);

  if (strcmp(component, name) != 0)
  {
    return false;
  }

  if (group_ptr == NULL)
  {
    group_ptr = parameter_ptr->group_ptr;
  }
  else
  {
    group_ptr = group_ptr->parent_group_ptr;
  }

  name = group_ptr->name;

  goto loop;
}

void
lv2dynparam_host_notify_parameter_appeared(
  struct lv2dynparam_host_instance * instance_ptr,
  struct lv2dynparam_host_parameter * parameter_ptr)
{
  struct list_head * node_ptr;
  struct lv2dynparam_host_parameter_pending_value_change * value_ptr;

  LOG_DEBUG("lv2dynparam_host_notify_parameter_appeared() called for \"%s\".", parameter_ptr->name);

  value_ptr = NULL;
  list_for_each(node_ptr, &instance_ptr->pending_parameter_value_changes)
  {
    value_ptr = list_entry(node_ptr, struct lv2dynparam_host_parameter_pending_value_change, siblings);
    if (parameter_asciizz_match(instance_ptr, parameter_ptr, value_ptr->name_asciizz))
    {
      list_del(node_ptr);
      break;
    }

    value_ptr = NULL;
  }

  if (value_ptr)
  {
    LOG_ERROR("Found value for parameter '%s'", parameter_ptr->name);
  }

  switch (parameter_ptr->type)
  {
  case LV2DYNPARAM_PARAMETER_TYPE_BOOLEAN:
    dynparam_parameter_boolean_appeared(
      parameter_ptr,
      instance_ptr->instance_ui_context,
      parameter_ptr->group_ptr->ui_context,
      parameter_ptr->name,
      &parameter_ptr->hints,
      parameter_ptr->data.boolean,
      &parameter_ptr->ui_context);
    break;
  case LV2DYNPARAM_PARAMETER_TYPE_FLOAT:
    dynparam_parameter_float_appeared(
      parameter_ptr,
      instance_ptr->instance_ui_context,
      parameter_ptr->group_ptr->ui_context,
      parameter_ptr->name,
      &parameter_ptr->hints,
      parameter_ptr->data.fpoint.value,
      parameter_ptr->data.fpoint.min,
      parameter_ptr->data.fpoint.max,
      &parameter_ptr->ui_context);
    break;
  case LV2DYNPARAM_PARAMETER_TYPE_ENUM:
    dynparam_parameter_enum_appeared(
      parameter_ptr,
      instance_ptr->instance_ui_context,
      parameter_ptr->group_ptr->ui_context,
      parameter_ptr->name,
      &parameter_ptr->hints,
      parameter_ptr->data.enumeration.selected_value,
      (const char * const *)parameter_ptr->data.enumeration.values,
      parameter_ptr->data.enumeration.values_count,
      &parameter_ptr->ui_context);
    break;
  case LV2DYNPARAM_PARAMETER_TYPE_INT:
    dynparam_parameter_int_appeared(
      parameter_ptr,
      instance_ptr->instance_ui_context,
      parameter_ptr->group_ptr->ui_context,
      parameter_ptr->name,
      &parameter_ptr->hints,
      parameter_ptr->data.integer.value,
      parameter_ptr->data.integer.min,
      parameter_ptr->data.integer.max,
      &parameter_ptr->ui_context);
    break;
  default:
    assert(0);                  /* unknown parameter type, should be ignored in host callback */
    break;
  }

  if (value_ptr != NULL)
  {
    if (value_ptr->type == LV2DYNPARAM_PARAMETER_TYPE_STRING)
    {
      rtsafe_memory_deallocate(value_ptr->data.string);
    }

    rtsafe_memory_deallocate(value_ptr->name_asciizz);
    rtsafe_memory_pool_deallocate(instance_ptr->pending_parameter_value_changes_pool, value_ptr);
  }
}

void
lv2dynparam_host_notify_parameter_disappeared(
  struct lv2dynparam_host_instance * instance_ptr,
  struct lv2dynparam_host_parameter * parameter_ptr)
{
  LOG_DEBUG("lv2dynparam_host_notify_parameter_disappeared() called for \"%s\".", parameter_ptr->name);

  switch (parameter_ptr->type)
  {
  case LV2DYNPARAM_PARAMETER_TYPE_BOOLEAN:
    dynparam_parameter_boolean_disappeared(
      instance_ptr->instance_ui_context,
      parameter_ptr->group_ptr->ui_context,
      parameter_ptr->ui_context);
    break;
  case LV2DYNPARAM_PARAMETER_TYPE_FLOAT:
    dynparam_parameter_float_disappeared(
      instance_ptr->instance_ui_context,
      parameter_ptr->group_ptr->ui_context,
      parameter_ptr->ui_context);
    break;
  case LV2DYNPARAM_PARAMETER_TYPE_ENUM:
    dynparam_parameter_enum_disappeared(
      instance_ptr->instance_ui_context,
      parameter_ptr->group_ptr->ui_context,
      parameter_ptr->ui_context);
    break;
  case LV2DYNPARAM_PARAMETER_TYPE_INT:
    dynparam_parameter_int_disappeared(
      instance_ptr->instance_ui_context,
      parameter_ptr->group_ptr->ui_context,
      parameter_ptr->ui_context);
    break;
  default:
    assert(0);                  /* unknown parameter type, should be ignored in host callback and assert on appear */
  }
}

/* called when ui is shown */
void
lv2dynparam_host_notify(
  struct lv2dynparam_host_instance * instance_ptr,
  struct lv2dynparam_host_group * group_ptr)
{
  struct list_head * node_ptr;
  struct list_head * temp_node_ptr;
  struct lv2dynparam_host_group * child_group_ptr;
  struct lv2dynparam_host_parameter * parameter_ptr;

  //LOG_DEBUG("Iterating \"%s\" groups begin", group_ptr->name);

  assert(instance_ptr->ui);

  list_for_each(node_ptr, &group_ptr->child_groups)
  {
    if (group_ptr->pending_childern_count == 0)
    {
      break;
    }

    child_group_ptr = list_entry(node_ptr, struct lv2dynparam_host_group, siblings);
    //LOG_DEBUG("host notify - group \"%s\"", child_group_ptr->name);

    switch (child_group_ptr->pending_state)
    {
    case LV2DYNPARAM_PENDING_APPEAR:
      /* UI knows nothing about this group - notify it */
      lv2dynparam_host_notify_group_appeared(
        instance_ptr,
        child_group_ptr);
      child_group_ptr->pending_state = LV2DYNPARAM_PENDING_NOTHING;
      lv2dynparam_host_group_pending_children_count_decrement(group_ptr);
      break;
    case LV2DYNPARAM_PENDING_NOTHING:
      break;
    case LV2DYNPARAM_PENDING_DISAPPEAR:
      lv2dynparam_host_notify_group_disappeared(
        instance_ptr,
        child_group_ptr);
      lv2dynparam_host_group_pending_children_count_decrement(group_ptr);
      list_del(&child_group_ptr->siblings);
      lv2dynparam_host_group_free(instance_ptr, child_group_ptr);
      break;
    default:
      LOG_ERROR("unknown pending_state %u of group \"%s\"", child_group_ptr->pending_state, child_group_ptr->name);
      assert(0);
    }

    lv2dynparam_host_notify(
      instance_ptr,
      child_group_ptr);
  }

  //LOG_DEBUG("Iterating \"%s\" groups end", group_ptr->name);
  //LOG_DEBUG("Iterating \"%s\" params begin", group_ptr->name);

  list_for_each_safe(node_ptr, temp_node_ptr, &group_ptr->child_params)
  {
    if (group_ptr->pending_childern_count == 0)
    {
      break;
    }

    parameter_ptr = list_entry(node_ptr, struct lv2dynparam_host_parameter, siblings);
    //LOG_DEBUG("host notify - parameter \"%s\"", parameter_ptr->name);

    switch (parameter_ptr->pending_state)
    {
    case LV2DYNPARAM_PENDING_APPEAR:
      lv2dynparam_host_notify_parameter_appeared(
        instance_ptr,
        parameter_ptr);
      parameter_ptr->pending_state = LV2DYNPARAM_PENDING_NOTHING;
      lv2dynparam_host_group_pending_children_count_decrement(group_ptr);
      break;
    case LV2DYNPARAM_PENDING_NOTHING:
      break;
    case LV2DYNPARAM_PENDING_DISAPPEAR:
      lv2dynparam_host_notify_parameter_disappeared(
        instance_ptr,
        parameter_ptr);
      parameter_ptr->pending_state = LV2DYNPARAM_PENDING_NOTHING;
      lv2dynparam_host_group_pending_children_count_decrement(group_ptr);
      list_del(&parameter_ptr->siblings);
      lv2dynparam_host_parameter_free(instance_ptr, parameter_ptr);
      break;
    default:
      LOG_ERROR("unknown pending_state %u of parameter \"%s\"", parameter_ptr->pending_state, parameter_ptr->name);
      assert(0);
    }
  }

  //LOG_DEBUG("Iterating \"%s\" params end", group_ptr->name);
}

/* called when ui going off */
void
lv2dynparam_host_group_hide(
  struct lv2dynparam_host_instance * instance_ptr,
  struct lv2dynparam_host_group * group_ptr)
{
  struct list_head * node_ptr;
  struct lv2dynparam_host_group * child_group_ptr;
  struct lv2dynparam_host_parameter * parameter_ptr;

  assert(!instance_ptr->ui);

  if (group_ptr->pending_state == LV2DYNPARAM_PENDING_APPEAR)
  {
    /* UI does not know about this group and thus cannot know about its childred too */
    return;
  }

  //LOG_DEBUG("Hidding group \"%s\" group", group_ptr->name);

  list_for_each(node_ptr, &group_ptr->child_params)
  {
    parameter_ptr = list_entry(node_ptr, struct lv2dynparam_host_parameter, siblings);

    if (parameter_ptr->pending_state != LV2DYNPARAM_PENDING_APPEAR)
    {
      //LOG_DEBUG("Hidding parameter \"%s\" group", parameter_ptr->name);

      lv2dynparam_host_notify_parameter_disappeared(
        instance_ptr,
        parameter_ptr);

      parameter_ptr->pending_state = LV2DYNPARAM_PENDING_APPEAR;
      lv2dynparam_host_group_pending_children_count_increment(group_ptr);
    }
  }

  list_for_each(node_ptr, &group_ptr->child_groups)
  {
    child_group_ptr = list_entry(node_ptr, struct lv2dynparam_host_group, siblings);

    lv2dynparam_host_group_hide(
      instance_ptr,
      child_group_ptr);

    lv2dynparam_host_group_pending_children_count_increment(group_ptr);
  }

  lv2dynparam_host_notify_group_disappeared(
    instance_ptr,
    group_ptr);

  group_ptr->pending_state = LV2DYNPARAM_PENDING_APPEAR;
}

static
bool
prepend_string(
  char ** buffer_ptr_ptr,
  size_t * buffer_size_ptr,
  char ** last_ptr_ptr,
  const char * string)
{
  size_t len;
  char * new_buffer_ptr;
  size_t new_buffer_size;
  char * new_last_ptr;

  len = strlen(string);

  if (len > *last_ptr_ptr - *buffer_ptr_ptr)
  {
    new_buffer_size = *buffer_size_ptr + len + SERIALIZE_BUFFER_INCREMENT_SIZE;

    new_buffer_ptr = malloc(new_buffer_size);
    if (new_buffer_ptr == NULL)
    {
      LOG_ERROR("malloc() failed to allocate %zu bytes", new_buffer_size);
      return false;
    }

    new_last_ptr = new_buffer_ptr + (new_buffer_size - *buffer_size_ptr) + (*last_ptr_ptr - *buffer_ptr_ptr);
    assert(*last_ptr_ptr - *buffer_ptr_ptr + len + SERIALIZE_BUFFER_INCREMENT_SIZE == new_last_ptr - new_buffer_ptr);

    strcpy(new_last_ptr, *last_ptr_ptr);

    free(*buffer_ptr_ptr);

    *buffer_ptr_ptr = new_buffer_ptr;
    *buffer_size_ptr = new_buffer_size;
    *last_ptr_ptr = new_last_ptr;
  }

  assert(len <= *last_ptr_ptr - *buffer_ptr_ptr);

  *last_ptr_ptr -= len;
  memcpy(*last_ptr_ptr, string, len);

  return true;
}

static
char *
strdup_escape(
  const char * string)
{
  char * buffer;
  const char * src_ptr;
  char * dest_ptr;
  size_t len;

  len = 0;
  for (src_ptr = string ; *src_ptr ; src_ptr++)
  {
    len++;

    if (*src_ptr == SERIALIZE_SEPARATOR_CHAR ||
        *src_ptr == SERIALIZE_ESCAPE_CHAR)
    {
      len++;
    }
  }
  len++;                        /* terminating zero char */

  buffer = malloc(len);
  if (buffer == NULL)
  {
    LOG_ERROR("malloc() failed to allocate %zu bytes", len);
    return NULL;
  }

  dest_ptr = buffer;
  for (src_ptr = string ; *src_ptr ; src_ptr++)
  {
    if (*src_ptr == SERIALIZE_SEPARATOR_CHAR ||
        *src_ptr == SERIALIZE_ESCAPE_CHAR)
    {
      *dest_ptr++ = SERIALIZE_ESCAPE_CHAR;
    }

    *dest_ptr++ = *src_ptr;
  }
  *dest_ptr = 0;

  return buffer;
}

static
void
dynparam_get_parameter(
  struct lv2dynparam_host_instance * instance_ptr,
  struct lv2dynparam_host_parameter * parameter_ptr,
  lv2dynparam_parameter_get_callback callback,
  void * context)
{
  struct lv2dynparam_host_group * group_ptr;
  char * buffer_ptr;
  size_t buffer_size;
  char * last_ptr;
  char separator[2] = {SERIALIZE_SEPARATOR_CHAR, '\0'};
  char * name;
  bool success;
  char value_str[100];
  const char * value_enum;
  char * value_buffer;
  const char * value;
  char * locale;

  buffer_size = SERIALIZE_BUFFER_INITIAL_SIZE;
  buffer_ptr = malloc(buffer_size);
  if (buffer_ptr == NULL)
  {
      LOG_ERROR("malloc() failed to allocate %zu bytes", buffer_size);
    goto exit;
  }

  last_ptr = buffer_ptr + buffer_size - 1;
  *last_ptr = 0;

  name = strdup_escape(parameter_ptr->name);
  if (name == NULL)
  {
    goto free;
  }

  success = prepend_string(&buffer_ptr, &buffer_size, &last_ptr, name);
  free(name);
  if (!success)
  {
    goto free;
  }

  for (group_ptr = parameter_ptr->group_ptr;
       group_ptr != instance_ptr->root_group_ptr;
       group_ptr = group_ptr->parent_group_ptr)
  {
    if (!prepend_string(&buffer_ptr, &buffer_size, &last_ptr, separator))
    {
      goto free;
    }

    name = strdup_escape(group_ptr->name);
    if (name == NULL)
    {
      goto free;
    }

    success = prepend_string(&buffer_ptr, &buffer_size, &last_ptr, name);
    free(name);
    if (!success)
    {
      goto free;
    }
  }

  locale = strdup(setlocale(LC_NUMERIC, NULL));
  setlocale(LC_NUMERIC, "POSIX");

  value_buffer = NULL;

  switch (parameter_ptr->type)
  {
  case LV2DYNPARAM_PARAMETER_TYPE_BOOLEAN:
    sprintf(value_str, "%c%s", SERIALIZE_TYPE_CHAR_BOOLEAN, parameter_ptr->data.boolean ? "true" : "false");
    value = value_str;
    break;
  case LV2DYNPARAM_PARAMETER_TYPE_FLOAT:
    sprintf(value_str, "%c%f", SERIALIZE_TYPE_CHAR_FLOAT, parameter_ptr->data.fpoint.value);
    value = value_str;
    break;
  case LV2DYNPARAM_PARAMETER_TYPE_ENUM:
    value_enum = parameter_ptr->data.enumeration.values[parameter_ptr->data.enumeration.selected_value];

    value_buffer = malloc(strlen(value_enum) + 2);
    if (value_buffer == NULL)
    {
      LOG_ERROR("failed to allocate memory for enum value buffer");
      setlocale(LC_NUMERIC, locale);
      free(locale);
      goto free;
    }

    value_buffer[0] = SERIALIZE_TYPE_CHAR_STRING;
    strcpy(value_buffer + 1, value_enum);
    value = value_buffer;
    break;
  case LV2DYNPARAM_PARAMETER_TYPE_INT:
    sprintf(value_str, "%c%i", SERIALIZE_TYPE_CHAR_INT, parameter_ptr->data.integer.value);
    value = value_str;
    break;
  default:
    assert(0);                  /* unknown parameter type, should be ignored in host callback */
    setlocale(LC_NUMERIC, locale);
    free(locale);
    goto free;
  }

  setlocale(LC_NUMERIC, locale);
  free(locale);

  LOG_DEBUG("Parameter '%s' with value '%s'", last_ptr, value);
  callback(context, last_ptr, value);

  if (value_buffer != NULL)
  {
    free(value_buffer);
  }

free:
  free(buffer_ptr);

exit:
  return;
}

static
void
dynparam_get_group_parameters(
  struct lv2dynparam_host_instance * instance_ptr,
  struct lv2dynparam_host_group * group_ptr,
  lv2dynparam_parameter_get_callback callback,
  void * context)
{
  struct list_head * node_ptr;
  struct lv2dynparam_host_group * child_group_ptr;
  struct lv2dynparam_host_parameter * parameter_ptr;

  //LOG_DEBUG("Iterating \"%s\" groups begin", group_ptr->name);

  list_for_each(node_ptr, &group_ptr->child_groups)
  {
    child_group_ptr = list_entry(node_ptr, struct lv2dynparam_host_group, siblings);

    //LOG_DEBUG("host notify - group \"%s\"", child_group_ptr->name);

    dynparam_get_group_parameters(instance_ptr, child_group_ptr, callback, context);
  }

  //LOG_DEBUG("Iterating \"%s\" groups end", group_ptr->name);
  //LOG_DEBUG("Iterating \"%s\" params begin", group_ptr->name);

  list_for_each(node_ptr, &group_ptr->child_params)
  {
    parameter_ptr = list_entry(node_ptr, struct lv2dynparam_host_parameter, siblings);

    //LOG_DEBUG("host notify - parameter \"%s\"", parameter_ptr->name);

    dynparam_get_parameter(instance_ptr, parameter_ptr, callback, context);
  }

  //LOG_DEBUG("Iterating \"%s\" params end", group_ptr->name);
}

static
struct lv2dynparam_host_parameter *
find_parameter_asciizz(
  struct lv2dynparam_host_instance * instance_ptr,
  const char * asciizz)
{
  const char * component;
  const char * next_component;
  size_t len;
  struct lv2dynparam_host_group * group_ptr;
  struct lv2dynparam_host_group * child_group_ptr;
  struct lv2dynparam_host_parameter * parameter_ptr;
  struct list_head * node_ptr;

  component = asciizz;
  group_ptr = instance_ptr->root_group_ptr;

next_component:
  if (*component == 0)
  {
    return NULL;
  }

  len = strlen(component);
  next_component = component + len + 1;

  if (*next_component != 0)
  {
    /* if this is not last component, we are searching for a group */

    list_for_each(node_ptr, &group_ptr->child_groups)
    {
      child_group_ptr = list_entry(node_ptr, struct lv2dynparam_host_group, siblings);
      if (strcmp(component, child_group_ptr->name) == 0)
      {
        group_ptr = child_group_ptr;
        component += len + 1;
        goto next_component;
      }
    }
  }
  else
  {
    /* if this is the last component, we are searching for a parameter  */

    list_for_each(node_ptr, &group_ptr->child_params)
    {
      parameter_ptr = list_entry(node_ptr, struct lv2dynparam_host_parameter, siblings);
      if (strcmp(component, parameter_ptr->name) == 0)
      {
        return parameter_ptr;
      }
    }
  }

  return NULL;
}

static
void
parameter_value_change(
  struct lv2dynparam_host_instance * instance_ptr,
  struct lv2dynparam_host_parameter * parameter_ptr,
  unsigned int value_type,
  union lv2dynparam_host_parameter_value * value_ptr)
{
  unsigned int i;

  if (value_ptr != &parameter_ptr->data)
  {
    if (parameter_ptr->type == LV2DYNPARAM_PARAMETER_TYPE_ENUM)
    {
      if (value_type == LV2DYNPARAM_PARAMETER_TYPE_STRING)
      {
        for (i = 0 ; i < value_ptr->enumeration.values_count ; i++)
        {
          if (strcmp(value_ptr->enumeration.values[i], value_ptr->string) == 0)
          {
            value_ptr->enumeration.selected_value = i;
            goto set;
          }
        }
      }
      else
      {
        goto push;
      }

      LOG_ERROR("Wrong value '%s' for enum parameter '%s'", value_ptr->string, parameter_ptr->name);
      return;
    }

  set:
    switch (parameter_ptr->type)
    {
    case LV2DYNPARAM_PARAMETER_TYPE_BOOLEAN:
      parameter_ptr->data.boolean = value_ptr->boolean;
      break;
    case LV2DYNPARAM_PARAMETER_TYPE_FLOAT:
      parameter_ptr->data.fpoint.value = value_ptr->fpoint.value;
      break;
    case LV2DYNPARAM_PARAMETER_TYPE_INT:
      parameter_ptr->data.integer.value = value_ptr->integer.value;
      break;
    case LV2DYNPARAM_PARAMETER_TYPE_ENUM:
      /* whoa? we should enter the "if type is enum" statement above */
      assert(0);
    default:
      LOG_ERROR("Parameter change for parameter of unknown type %u received", parameter_ptr->type);
      return;
    }
  }

push:
  switch (parameter_ptr->type)
  {
  case LV2DYNPARAM_PARAMETER_TYPE_BOOLEAN:
    *((unsigned char *)parameter_ptr->value_ptr) = parameter_ptr->data.boolean ? 1 : 0;
    break;
  case LV2DYNPARAM_PARAMETER_TYPE_FLOAT:
    *((float *)parameter_ptr->value_ptr) = parameter_ptr->data.fpoint.value;
    break;
  case LV2DYNPARAM_PARAMETER_TYPE_ENUM:
    *((unsigned int *)parameter_ptr->value_ptr) = parameter_ptr->data.enumeration.selected_value;
    break;
  case LV2DYNPARAM_PARAMETER_TYPE_INT:
    *((signed int *)parameter_ptr->value_ptr) = parameter_ptr->data.integer.value;
    break;
  default:
    LOG_ERROR("Parameter change for parameter of unknown type %u received", parameter_ptr->type);
    return;
  }

  instance_ptr->callbacks_ptr->parameter_change(parameter_ptr->param_handle);
}

static
void
free_parameter_pending_value_change(
  struct lv2dynparam_host_instance * instance_ptr,
  struct lv2dynparam_host_parameter_pending_value_change * value_ptr,
  bool free_data_pointers)
{
  rtsafe_memory_deallocate(value_ptr->name_asciizz);

  if (free_data_pointers)
  {
    if (value_ptr->type == LV2DYNPARAM_PARAMETER_TYPE_STRING)
    {
      rtsafe_memory_deallocate(value_ptr->data.string);
    }
  }

  rtsafe_memory_pool_deallocate(instance_ptr->pending_parameter_value_changes_pool, value_ptr);
}

#define instance_ptr ((struct lv2dynparam_host_instance *)instance)

void
lv2dynparam_host_realtime_run(
  lv2dynparam_host_instance instance)
{
  struct list_head * node_ptr;
  struct lv2dynparam_host_message * message_ptr;
  struct lv2dynparam_host_parameter * parameter_ptr;
  struct lv2dynparam_host_parameter_pending_value_change * value_ptr;

  if (!audiolock_enter_audio(instance_ptr->lock))
  {
    /* we are not lucky enough - ui thread, is accessing the protected data */
    return;
  }

  while (!list_empty(&instance_ptr->ui_to_realtime_queue))
  {
    node_ptr = instance_ptr->ui_to_realtime_queue.next;
    message_ptr = list_entry(node_ptr, struct lv2dynparam_host_message, siblings);

    switch (message_ptr->message_type)
    {
    case LV2DYNPARAM_HOST_MESSAGE_TYPE_PARAMETER_CHANGE:
      parameter_ptr = message_ptr->context.parameter;
      parameter_value_change(instance_ptr, parameter_ptr, parameter_ptr->type, &parameter_ptr->data);
      break;

    case LV2DYNPARAM_HOST_MESSAGE_TYPE_UNKNOWN_PARAMETER_CHANGE:
      value_ptr = message_ptr->context.value_change;

      parameter_ptr = find_parameter_asciizz(instance_ptr, value_ptr->name_asciizz);
      if (parameter_ptr != NULL)
      {
        LOG_DEBUG("Applying pending parameter '%s' value change", parameter_ptr->name);
        parameter_value_change(instance_ptr, parameter_ptr, value_ptr->type, &value_ptr->data);
        free_parameter_pending_value_change(
          instance_ptr,
          value_ptr,
          parameter_ptr->type == LV2DYNPARAM_PARAMETER_TYPE_ENUM &&
          value_ptr->type == LV2DYNPARAM_PARAMETER_TYPE_STRING);
      }
      else
      {
        LOG_DEBUG("Postponing pending parameter value change");
        /* TODO: nobody is reading these changes, we dont really have test case for this yet */
        /* It should probably be read in host_callbacks.c lv2dynparam_host_parameter_appear() */
        list_add_tail(&value_ptr->siblings, &instance_ptr->pending_parameter_value_changes);
      }

      break;

    default:
       LOG_ERROR("Message of unknown type %u received", message_ptr->message_type);
     }

    list_del(node_ptr);
    rtsafe_memory_pool_deallocate(instance_ptr->messages_pool, message_ptr);
  }

  audiolock_leave_audio(instance_ptr->lock);
}

void
lv2dynparam_host_ui_run(
  lv2dynparam_host_instance instance)
{
  //LOG_DEBUG("lv2dynparam_host_ui_run() called.");

  audiolock_enter_ui(instance_ptr->lock);

  if (instance_ptr->ui)         /* we have nothing to do if there is no ui shown */
  {
    /* At this point we should have the root group appeared and gui-referenced,
       because it appears on host attach that is called before lv2dynparam_host_ui_on()
       and because lv2dynparam_host_ui_on() will gui-reference it. */
    assert(instance_ptr->root_group_ptr != NULL);
    assert(instance_ptr->root_group_ptr->pending_state == LV2DYNPARAM_PENDING_NOTHING);
    //LOG_DEBUG("pending_childern_count is %u", instance_ptr->root_group_ptr->pending_childern_count);

    if (instance_ptr->root_group_ptr->pending_childern_count != 0)
    {
      lv2dynparam_host_notify(
        instance_ptr,
        instance_ptr->root_group_ptr);

      assert(instance_ptr->root_group_ptr->pending_childern_count == 0);
    }
  }

  audiolock_leave_ui(instance_ptr->lock);
}

void
lv2dynparam_host_ui_on(
  lv2dynparam_host_instance instance)
{
  audiolock_enter_ui(instance_ptr->lock);

  if (!instance_ptr->ui)
  {
    assert(instance_ptr->root_group_ptr != NULL); /* root group appears on host_attach */

    LOG_DEBUG("UI on - notifying for new things.");
    LOG_DEBUG("pending_childern_count is %u", instance_ptr->root_group_ptr->pending_childern_count);

    instance_ptr->ui = true;

    /* UI knows nothing about root group - notify it */
    //LOG_DEBUG("pending_childern_count is %u", instance_ptr->root_group_ptr->pending_childern_count);
    assert(instance_ptr->root_group_ptr->pending_state == LV2DYNPARAM_PENDING_APPEAR);
    lv2dynparam_host_notify_group_appeared(
      instance_ptr,
      instance_ptr->root_group_ptr);
    instance_ptr->root_group_ptr->pending_state = LV2DYNPARAM_PENDING_NOTHING;

    lv2dynparam_host_notify(
      instance_ptr,
      instance_ptr->root_group_ptr);

    LOG_DEBUG("pending_childern_count is %u", instance_ptr->root_group_ptr->pending_childern_count);
    assert(instance_ptr->root_group_ptr->pending_childern_count == 0);
  }

  audiolock_leave_ui(instance_ptr->lock);
}

void
lv2dynparam_host_ui_off(
  lv2dynparam_host_instance instance)
{
  audiolock_enter_ui(instance_ptr->lock);

  LOG_DEBUG("UI off - removing known things.");
  //LOG_DEBUG("pending_childern_count is %u", instance_ptr->root_group_ptr->pending_childern_count);

  instance_ptr->ui = false;

  assert(instance_ptr->root_group_ptr != NULL); /* root group appears on host_attach */

  lv2dynparam_host_group_hide(
    instance_ptr,
    instance_ptr->root_group_ptr);

  //LOG_DEBUG("pending_childern_count is %u", instance_ptr->root_group_ptr->pending_childern_count);

  audiolock_leave_ui(instance_ptr->lock);
}

void
lv2dynparam_host_detach(
  lv2dynparam_host_instance instance)
{
  /* TODO:
   * - deallocate resources in host tree
   * - free per instance resources allocated in lv2dynparam_host_attach()
   */
}

#define parameter_ptr ((struct lv2dynparam_host_parameter *)parameter_handle)

void
lv2dynparam_parameter_boolean_change(
  lv2dynparam_host_instance instance,
  lv2dynparam_host_parameter parameter_handle,
  bool value)
{
  struct lv2dynparam_host_message * message_ptr;

  audiolock_enter_ui(instance_ptr->lock);

  LOG_DEBUG("\"%s\" changed to \"%s\"", parameter_ptr->name, value ? "TRUE" : "FALSE");

  message_ptr = rtsafe_memory_pool_allocate_sleepy(instance_ptr->messages_pool);

  parameter_ptr->data.boolean = value;

  message_ptr->message_type = LV2DYNPARAM_HOST_MESSAGE_TYPE_PARAMETER_CHANGE;

  message_ptr->context.parameter = parameter_ptr;

  list_add_tail(&message_ptr->siblings, &instance_ptr->ui_to_realtime_queue);

  audiolock_leave_ui(instance_ptr->lock);
}

void
lv2dynparam_parameter_float_change(
  lv2dynparam_host_instance instance,
  lv2dynparam_host_parameter parameter_handle,
  float value)
{
  struct lv2dynparam_host_message * message_ptr;

  audiolock_enter_ui(instance_ptr->lock);

  LOG_DEBUG("\"%s\" changed to %f", parameter_ptr->name, value);

  message_ptr = rtsafe_memory_pool_allocate_sleepy(instance_ptr->messages_pool);

  parameter_ptr->data.fpoint.value = value;

  message_ptr->message_type = LV2DYNPARAM_HOST_MESSAGE_TYPE_PARAMETER_CHANGE;

  message_ptr->context.parameter = parameter_ptr;

  list_add_tail(&message_ptr->siblings, &instance_ptr->ui_to_realtime_queue);

  audiolock_leave_ui(instance_ptr->lock);
}

void
lv2dynparam_parameter_enum_change(
  lv2dynparam_host_instance instance,
  lv2dynparam_host_parameter parameter_handle,
  unsigned int selected_index_value)
{
  struct lv2dynparam_host_message * message_ptr;

  audiolock_enter_ui(instance_ptr->lock);

  LOG_DEBUG("\"%s\" changed to \"%s\" (index %u)", parameter_ptr->name, parameter_ptr->data.enumeration.values[selected_index_value], selected_index_value);

  message_ptr = rtsafe_memory_pool_allocate_sleepy(instance_ptr->messages_pool);

  parameter_ptr->data.enumeration.selected_value = selected_index_value;

  message_ptr->message_type = LV2DYNPARAM_HOST_MESSAGE_TYPE_PARAMETER_CHANGE;

  message_ptr->context.parameter = parameter_ptr;

  list_add_tail(&message_ptr->siblings, &instance_ptr->ui_to_realtime_queue);

  audiolock_leave_ui(instance_ptr->lock);
}

void
lv2dynparam_parameter_int_change(
  lv2dynparam_host_instance instance,
  lv2dynparam_host_parameter parameter_handle,
  signed int value)
{
  struct lv2dynparam_host_message * message_ptr;

  audiolock_enter_ui(instance_ptr->lock);

  LOG_DEBUG("\"%s\" changed to %d", parameter_ptr->name, value);

  message_ptr = rtsafe_memory_pool_allocate_sleepy(instance_ptr->messages_pool);

  parameter_ptr->data.integer.value = value;

  message_ptr->message_type = LV2DYNPARAM_HOST_MESSAGE_TYPE_PARAMETER_CHANGE;

  message_ptr->context.parameter = parameter_ptr;

  list_add_tail(&message_ptr->siblings, &instance_ptr->ui_to_realtime_queue);

  audiolock_leave_ui(instance_ptr->lock);
}

void
lv2dynparam_get_parameters(
  lv2dynparam_host_instance instance,
  lv2dynparam_parameter_get_callback callback,
  void * context)
{
  audiolock_enter_ui(instance_ptr->lock);

  if (instance_ptr->root_group_ptr != NULL)
  {
    dynparam_get_group_parameters(instance_ptr, instance_ptr->root_group_ptr, callback, context);
  }

  audiolock_leave_ui(instance_ptr->lock);
}

char *
string_unescape(
  lv2dynparam_host_instance instance,
  const char * string)
{
  char * buffer;
  size_t len;
  char * dest_ptr;
  const char * src_ptr;
  bool escape;

  //LOG_DEBUG("unescaping '%s'", string);

  len = 0;
  escape = false;
  for (src_ptr = string ; *src_ptr ; src_ptr++)
  {
    if (*src_ptr == SERIALIZE_ESCAPE_CHAR && !escape)
    {
      escape = true;
      continue;
    }

    len++;
    escape = false;
  }
  len += 2;                     /* terminating zero chars */

  //LOG_DEBUG("len = %zu", len);

  buffer = rtsafe_memory_allocate_sleepy(instance_ptr->memory, len);
  if (buffer == NULL)
  {
    LOG_ERROR("Failed to allocate %zu bytes", len);
    return NULL;
  }

  escape = false;
  dest_ptr = buffer;
  for (src_ptr = string ; *src_ptr ; src_ptr++)
  {
    //LOG_DEBUG("char '%c'", *src_ptr);
    if (*src_ptr == SERIALIZE_ESCAPE_CHAR && !escape)
    {
      //LOG_DEBUG("escape");
      escape = true;
      continue;
    }

    if (*src_ptr == SERIALIZE_SEPARATOR_CHAR && !escape)
    {
      //LOG_DEBUG("separator");
      assert(len > 0);
      len--;
      *dest_ptr++ = 0;
      continue;
    }

    assert(len > 0);
    len--;
    *dest_ptr++ = *src_ptr;
    escape = false;
  }

  //LOG_DEBUG("term1");
  assert(len > 0);
  len--;
  dest_ptr[0] = 0;

  //LOG_DEBUG("term2");
  assert(len > 0);
  len--;
  dest_ptr[1] = 0;

  return buffer;
}

#undef parameter_ptr

static
unsigned int
set_parameter(
  lv2dynparam_host_instance instance,
  const char * parameter_name,
  const char * parameter_value,
  const unsigned int * type_ptr,
  union lv2dynparam_host_parameter_value * value_ptr)
{
  char * locale;
  unsigned int i;
  unsigned int selected_index;
  unsigned int selected_index_found;
  char typechar;
  unsigned int type;

  typechar = *parameter_value;
  parameter_value++;

  switch (typechar)
  {
  case SERIALIZE_TYPE_CHAR_BOOLEAN:
    type = LV2DYNPARAM_PARAMETER_TYPE_BOOLEAN;
    break;
  case SERIALIZE_TYPE_CHAR_FLOAT:
    type = LV2DYNPARAM_PARAMETER_TYPE_FLOAT;
    break;
  case SERIALIZE_TYPE_CHAR_STRING:
    type = LV2DYNPARAM_PARAMETER_TYPE_ENUM;
    break;
  case SERIALIZE_TYPE_CHAR_INT:
    type = LV2DYNPARAM_PARAMETER_TYPE_INT;
    break;
  default:
    LOG_ERROR("Unknown parameter type char '%c'", typechar);
    return LV2DYNPARAM_PARAMETER_TYPE_UNKNOWN;
  }

  if (type_ptr != NULL && *type_ptr != type)
  {
    LOG_ERROR("Wrong value '%s' type '%c' for parameter '%s'", parameter_value, typechar, parameter_name);
    return LV2DYNPARAM_PARAMETER_TYPE_UNKNOWN;
  }

  locale = strdup(setlocale(LC_NUMERIC, NULL));
  setlocale(LC_NUMERIC, "POSIX");

  switch (type)
  {
  case LV2DYNPARAM_PARAMETER_TYPE_BOOLEAN:
    if (strcmp(parameter_value, "true") == 0)
    {
      value_ptr->boolean = true;
    }
    else if (strcmp(parameter_value, "false") == 0)
    {
      value_ptr->boolean = false;
    }
    else
    {
      LOG_ERROR("Wrong value '%s' for boolean parameter '%s'", parameter_value, parameter_name);
      type = LV2DYNPARAM_PARAMETER_TYPE_UNKNOWN;
    }
    break;
  case LV2DYNPARAM_PARAMETER_TYPE_FLOAT:
    if (sscanf(parameter_value, "%f", &value_ptr->fpoint.value) != 1)
    {
      LOG_ERROR("failed to convert value '%s' of parameter '%s' to float", parameter_value, parameter_name);
      type = LV2DYNPARAM_PARAMETER_TYPE_UNKNOWN;
    }
    break;
  case LV2DYNPARAM_PARAMETER_TYPE_ENUM:
    if (type_ptr != NULL)
    {
      selected_index_found = false;
      for (i = 0 ; i < value_ptr->enumeration.values_count ; i++)
      {
        if (strcmp(value_ptr->enumeration.values[i], parameter_value) == 0)
        {
          selected_index = i;
          selected_index_found = true;
          break;
        }
      }

      if (!selected_index_found)
      {
        LOG_ERROR("Wrong value '%s' for enum parameter '%s'", parameter_value, parameter_name);
        type = LV2DYNPARAM_PARAMETER_TYPE_UNKNOWN;
      }
      else
      {
        value_ptr->enumeration.selected_value = selected_index;
      }

      break;
    }
    /* fall through */
  case LV2DYNPARAM_PARAMETER_TYPE_STRING:
    value_ptr->string = lv2dynparam_strdup_sleepy(instance_ptr->memory, parameter_value);
    if (value_ptr->string == NULL)
    {
      LOG_ERROR("lv2dynparam_strdup_sleepy() failed");
      type = LV2DYNPARAM_PARAMETER_TYPE_UNKNOWN;
    }
    break;
  case LV2DYNPARAM_PARAMETER_TYPE_INT:
    if (sscanf(parameter_value, "%i", &value_ptr->integer.value) != 1)
    {
      LOG_ERROR("failed to convert value '%s' of parameter '%s' to signed int", parameter_value, parameter_name);
      type = LV2DYNPARAM_PARAMETER_TYPE_UNKNOWN;
    }
    break;
  default:
    LOG_ERROR("Parameter change for parameter of unknown type %u received", type);
    type = LV2DYNPARAM_PARAMETER_TYPE_UNKNOWN;
  }

  setlocale(LC_NUMERIC, locale);
  free(locale);

  return type;
}

void
lv2dynparam_set_parameter(
  lv2dynparam_host_instance instance,
  const char * parameter_name,
  const char * parameter_value)
{
  char * parameter_name_asciizz;
  struct lv2dynparam_host_message * message_ptr;
  struct lv2dynparam_host_parameter_pending_value_change * value_ptr;

  parameter_name_asciizz = string_unescape(instance, parameter_name);
  if (parameter_name_asciizz == NULL)
  {
    goto exit;
  }

  audiolock_enter_ui(instance_ptr->lock);

  message_ptr = rtsafe_memory_pool_allocate_sleepy(instance_ptr->messages_pool);
  if (message_ptr == NULL)
  {
    LOG_ERROR("failed to allocate memory for host message");
    goto free_name;
  }

  message_ptr = rtsafe_memory_pool_allocate_sleepy(instance_ptr->messages_pool);
  message_ptr->message_type = LV2DYNPARAM_HOST_MESSAGE_TYPE_UNKNOWN_PARAMETER_CHANGE;

  value_ptr = rtsafe_memory_pool_allocate_sleepy(instance_ptr->pending_parameter_value_changes_pool);
  if (value_ptr == NULL)
  {
    LOG_ERROR("failed to allocate memory for pending parameter value change");
    goto free_message;
  }

  message_ptr->context.value_change = value_ptr;
  value_ptr->type = set_parameter(instance, parameter_name, parameter_value, NULL, &value_ptr->data);
  if (value_ptr->type != LV2DYNPARAM_PARAMETER_TYPE_UNKNOWN)
  {
    LOG_DEBUG("Pending parameter '%s' value change to '%s' (%c)", parameter_name, parameter_value + 1, *parameter_value);
    value_ptr->name_asciizz = parameter_name_asciizz;
    list_add_tail(&message_ptr->siblings, &instance_ptr->ui_to_realtime_queue);
    goto unlock;
  }

  rtsafe_memory_pool_deallocate(instance_ptr->pending_parameter_value_changes_pool, value_ptr);

free_message:
  rtsafe_memory_pool_deallocate(instance_ptr->messages_pool, message_ptr);

free_name:
  rtsafe_memory_deallocate(parameter_name_asciizz);

unlock:
  audiolock_leave_ui(instance_ptr->lock);

exit:
  return;
}
