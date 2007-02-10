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
#include <assert.h>

#include "../lv2.h"
#include "../lv2dynparam.h"
#include "host.h"
#include "../audiolock.h"
#include "../list.h"
#include "../memory_atomic.h"
#include "internal.h"
#include "host_callbacks.h"
#include "../helpers.h"

#define LOG_LEVEL LOG_LEVEL_ERROR
#include "../log.h"

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

  lv2dynparam_memory_pool_deallocate(instance_ptr->parameters_pool, parameter_ptr);
}

BOOL
lv2dynparam_host_map_type_uri(
  struct lv2dynparam_host_parameter * parameter_ptr)
{
  if (strcmp(parameter_ptr->type_uri, LV2DYNPARAM_PARAMETER_TYPE_BOOLEAN_URI) == 0)
  {
    parameter_ptr->type = LV2DYNPARAM_PARAMETER_TYPE_BOOLEAN;
    return TRUE;
  }

  if (strcmp(parameter_ptr->type_uri, LV2DYNPARAM_PARAMETER_TYPE_FLOAT_URI) == 0)
  {
    parameter_ptr->type = LV2DYNPARAM_PARAMETER_TYPE_FLOAT;
    return TRUE;
  }

  if (strcmp(parameter_ptr->type_uri, LV2DYNPARAM_PARAMETER_TYPE_ENUM_URI) == 0)
  {
    parameter_ptr->type = LV2DYNPARAM_PARAMETER_TYPE_ENUM;
    return TRUE;
  }

  return FALSE;
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

int
lv2dynparam_host_add_synth(
  const LV2_Descriptor * lv2descriptor,
  LV2_Handle lv2instance,
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

  if (!lv2dynparam_memory_pool_create(
        sizeof(struct lv2dynparam_host_group),
        100,
        1000,
        &instance_ptr->groups_pool))
  {
    goto fail_destroy_lock;
  }

  if (!lv2dynparam_memory_pool_create(
        sizeof(struct lv2dynparam_host_parameter),
        100,
        1000,
        &instance_ptr->parameters_pool))
  {
    goto fail_destroy_groups_pool;
  }

  if (!lv2dynparam_memory_pool_create(
        sizeof(struct lv2dynparam_host_message),
        100,
        1000,
        &instance_ptr->messages_pool))
  {
    goto fail_destroy_parameters_pool;
  }

  if (!lv2dynparam_memory_init(
        64 * 1024,
        100,
        1000,
        &instance_ptr->memory))
  {
    goto fail_destroy_messages_pool;
  }

  instance_ptr->callbacks_ptr = lv2descriptor->extension_data(LV2DYNPARAM_URI);
  if (instance_ptr->callbacks_ptr == NULL)
  {
    /* Oh my! plugin does not support dynparam extension! This should be pre-checked by caller! */
    goto fail_uninit_memory;
  }

  INIT_LIST_HEAD(&instance_ptr->realtime_to_ui_queue);
  INIT_LIST_HEAD(&instance_ptr->ui_to_realtime_queue);
  instance_ptr->lv2instance = lv2instance;
  instance_ptr->root_group_ptr = NULL;
  instance_ptr->ui = FALSE;

  if (!instance_ptr->callbacks_ptr->host_attach(
        lv2instance,
        &g_lv2dynparam_host_callbacks,
        instance_ptr))
  {
    LOG_ERROR("lv2dynparam host_attach() failed.");
    goto fail_uninit_memory;
  }

  *instance_handle_ptr = (lv2dynparam_host_instance)instance_ptr;

  return 1;

fail_uninit_memory:
  lv2dynparam_memory_uninit(instance_ptr->memory);

fail_destroy_messages_pool:
  lv2dynparam_memory_pool_destroy(instance_ptr->messages_pool);

fail_destroy_parameters_pool:
  lv2dynparam_memory_pool_destroy(instance_ptr->parameters_pool);

fail_destroy_groups_pool:
  lv2dynparam_memory_pool_destroy(instance_ptr->groups_pool);

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

  //LOG_DEBUG("lv2dynparam_host_notify_group_appeared() called.");

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
    group_ptr->type_uri,
    &group_ptr->ui_context);
}

void
lv2dynparam_host_notify_group_disappeared(
  struct lv2dynparam_host_instance * instance_ptr,
  struct lv2dynparam_host_group * group_ptr)
{
  void * parent_group_ui_context;

  //LOG_DEBUG("lv2dynparam_host_notify_group_disappeared() called.");

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

void
lv2dynparam_host_notify_parameter_appeared(
  struct lv2dynparam_host_instance * instance_ptr,
  struct lv2dynparam_host_parameter * parameter_ptr)
{
  LOG_DEBUG("lv2dynparam_host_notify_group_appeared() called for \"%s\".", parameter_ptr->name);

  switch (parameter_ptr->type)
  {
  case LV2DYNPARAM_PARAMETER_TYPE_BOOLEAN:
    dynparam_parameter_boolean_appeared(
      parameter_ptr,
      instance_ptr->instance_ui_context,
      parameter_ptr->group_ptr->ui_context,
      parameter_ptr->name,
      parameter_ptr->data.boolean,
      &parameter_ptr->ui_context);
    break;
  case LV2DYNPARAM_PARAMETER_TYPE_FLOAT:
    dynparam_parameter_float_appeared(
      parameter_ptr,
      instance_ptr->instance_ui_context,
      parameter_ptr->group_ptr->ui_context,
      parameter_ptr->name,
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
      parameter_ptr->data.enumeration.selected_value,
      (const char * const *)parameter_ptr->data.enumeration.values,
      parameter_ptr->data.enumeration.values_count,
      &parameter_ptr->ui_context);
    break;
  default:
    assert(0);                  /* unknown parameter type, should be ignored in host callback */
    break;
  }
}

void
lv2dynparam_host_notify_parameter_disappeared(
  struct lv2dynparam_host_instance * instance_ptr,
  struct lv2dynparam_host_parameter * parameter_ptr)
{
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

#define instance_ptr ((struct lv2dynparam_host_instance *)instance)

void
lv2dynparam_host_realtime_run(
  lv2dynparam_host_instance instance)
{
  struct list_head * node_ptr;
  struct lv2dynparam_host_message * message_ptr;
  struct lv2dynparam_host_parameter * parameter_ptr;

  if (!audiolock_enter_audio(instance_ptr->lock))
  {
    /* we are not lucky enough - ui thread, is accessing the protected data */
    return;
  }

  while (!list_empty(&instance_ptr->ui_to_realtime_queue))
  {
    node_ptr = instance_ptr->ui_to_realtime_queue.next;
    list_del(node_ptr);
    message_ptr = list_entry(node_ptr, struct lv2dynparam_host_message, siblings);

    switch (message_ptr->message_type)
    {
    case LV2DYNPARAM_HOST_MESSAGE_TYPE_PARAMETER_CHANGE:
      parameter_ptr = message_ptr->context.parameter;

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
      default:
        LOG_ERROR("Parameter change for parameter of unknown type %u received", parameter_ptr->type);
      }

      instance_ptr->callbacks_ptr->parameter_change(parameter_ptr->param_handle);
      break;
    default:
       LOG_ERROR("Message of unknown type %u received", message_ptr->message_type);
     }

    lv2dynparam_memory_pool_deallocate(instance_ptr->messages_pool, message_ptr);
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
       because it appears and host attach that is called before lv2dynparam_host_ui_on()
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
    //LOG_DEBUG("pending_childern_count is %u", instance_ptr->root_group_ptr->pending_childern_count);

    instance_ptr->ui = TRUE;

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

    //LOG_DEBUG("pending_childern_count is %u", instance_ptr->root_group_ptr->pending_childern_count);
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

  instance_ptr->ui = FALSE;

  assert(instance_ptr->root_group_ptr != NULL); /* root group appears on host_attach */

  lv2dynparam_host_group_hide(
    instance_ptr,
    instance_ptr->root_group_ptr);

  //LOG_DEBUG("pending_childern_count is %u", instance_ptr->root_group_ptr->pending_childern_count);

  audiolock_leave_ui(instance_ptr->lock);
}

#define parameter_ptr ((struct lv2dynparam_host_parameter *)parameter_handle)

void
dynparam_parameter_boolean_change(
  lv2dynparam_host_instance instance,
  lv2dynparam_host_parameter parameter_handle,
  BOOL value)
{
  struct lv2dynparam_host_message * message_ptr;

  audiolock_enter_ui(instance_ptr->lock);

  LOG_DEBUG("\"%s\" changed to \"%s\"", parameter_ptr->name, value ? "TRUE" : "FALSE");

  message_ptr = lv2dynparam_memory_pool_allocate_sleepy(instance_ptr->messages_pool);

  parameter_ptr->data.boolean = value;

  message_ptr->message_type = LV2DYNPARAM_HOST_MESSAGE_TYPE_PARAMETER_CHANGE;

  message_ptr->context.parameter = parameter_ptr;

  list_add_tail(&message_ptr->siblings, &instance_ptr->ui_to_realtime_queue);

  audiolock_leave_ui(instance_ptr->lock);
}

void
dynparam_parameter_float_change(
  lv2dynparam_host_instance instance,
  lv2dynparam_host_parameter parameter_handle,
  float value)
{
  struct lv2dynparam_host_message * message_ptr;

  audiolock_enter_ui(instance_ptr->lock);

  LOG_DEBUG("\"%s\" changed to %f", parameter_ptr->name, value);

  message_ptr = lv2dynparam_memory_pool_allocate_sleepy(instance_ptr->messages_pool);

  parameter_ptr->data.fpoint.value = value;

  message_ptr->message_type = LV2DYNPARAM_HOST_MESSAGE_TYPE_PARAMETER_CHANGE;

  message_ptr->context.parameter = parameter_ptr;

  list_add_tail(&message_ptr->siblings, &instance_ptr->ui_to_realtime_queue);

  audiolock_leave_ui(instance_ptr->lock);
}

void
dynparam_parameter_enum_change(
  lv2dynparam_host_instance instance,
  lv2dynparam_host_parameter parameter_handle,
  unsigned int selected_index_value)
{
  struct lv2dynparam_host_message * message_ptr;

  audiolock_enter_ui(instance_ptr->lock);

  LOG_DEBUG("\"%s\" changed to \"%s\" (index %u)", parameter_ptr->name, parameter_ptr->data.enumeration.values[selected_index_value], selected_index_value);

  message_ptr = lv2dynparam_memory_pool_allocate_sleepy(instance_ptr->messages_pool);

  parameter_ptr->data.enumeration.selected_value = selected_index_value;

  message_ptr->message_type = LV2DYNPARAM_HOST_MESSAGE_TYPE_PARAMETER_CHANGE;

  message_ptr->context.parameter = parameter_ptr;

  list_add_tail(&message_ptr->siblings, &instance_ptr->ui_to_realtime_queue);

  audiolock_leave_ui(instance_ptr->lock);
}
