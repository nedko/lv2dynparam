/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of lv2dynparam host library
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

#ifndef DYNPARAM_INTERNAL_H__86778596_B1A9_4BD7_A14A_BECBD5589468__INCLUDED
#define DYNPARAM_INTERNAL_H__86778596_B1A9_4BD7_A14A_BECBD5589468__INCLUDED

#define LV2DYNPARAM_PENDING_NOTHING    0 /* nothing pending */
#define LV2DYNPARAM_PENDING_APPEAR     1 /* pending appear */
#define LV2DYNPARAM_PENDING_DISAPPEAR  2 /* pending disappear */

struct lv2dynparam_host_group
{
  struct list_head siblings;
  struct lv2dynparam_host_group * parent_group_ptr;
  lv2dynparam_group_handle group_handle;

  struct list_head child_groups;
  struct list_head child_params;
  struct list_head child_commands;

  char name[LV2DYNPARAM_MAX_STRING_SIZE];

  struct lv2dynparam_hints hints;

  unsigned int pending_state;
  unsigned int pending_childern_count;

  void * ui_context;
};

struct lv2dynparam_host_parameter
{
  struct list_head siblings;
  struct lv2dynparam_host_group * group_ptr;
  lv2dynparam_parameter_handle param_handle;
  char name[LV2DYNPARAM_MAX_STRING_SIZE];
  struct lv2dynparam_hints hints;
  char type_uri[LV2DYNPARAM_MAX_STRING_SIZE];
  unsigned int type;

  void * value_ptr;
  void * min_ptr;
  void * max_ptr;

  union lv2dynparam_host_parameter_range range;
  union lv2dynparam_host_parameter_value value;

  unsigned int pending_state;
  bool pending_value_change;

  bool context_set;
  void * context;               /* associated on create callback */

  void * context_pending_value_change; /* associated with pending value change */

  void * ui_context;            /* associated with UI (appear) */
};

struct lv2dynparam_host_command
{
  struct list_head siblings;
  struct lv2dynparam_host_group * group_ptr;
  lv2dynparam_command_handle command_handle;
  char name[LV2DYNPARAM_MAX_STRING_SIZE];

  bool gui_referenced;
  void * ui_context;
};

struct lv2dynparam_host_parameter_pending_value_change
{
  struct list_head siblings;
  char * name_asciizz;
  unsigned int type;
  union lv2dynparam_host_parameter_value data;
  void * context;
};

#define LV2DYNPARAM_HOST_MESSAGE_TYPE_PARAMETER_CHANGE          0
#define LV2DYNPARAM_HOST_MESSAGE_TYPE_COMMAND_EXECUTE           1
#define LV2DYNPARAM_HOST_MESSAGE_TYPE_UNKNOWN_PARAMETER_CHANGE  2

struct lv2dynparam_host_message
{
  struct list_head siblings;

  unsigned int message_type;

  union
  {
    struct lv2dynparam_host_group * group;
    struct lv2dynparam_host_parameter * parameter;
    struct lv2dynparam_host_command * command;
    struct lv2dynparam_host_parameter_pending_value_change * value_change;
  } context;
};

struct lv2dynparam_host_instance
{
  void * instance_context;
  audiolock_handle lock;
  const struct lv2dynparam_plugin_callbacks * callbacks_ptr;
  LV2_Handle lv2instance;

  struct lv2dynparam_host_group * root_group_ptr;

  bool ui;

  struct list_head realtime_to_ui_queue; /* protected by the audiolock */
  struct list_head ui_to_realtime_queue; /* protected by the audiolock */

  struct list_head pending_parameter_value_changes;

  rtsafe_memory_handle memory;

  rtsafe_memory_pool_handle groups_pool;
  rtsafe_memory_pool_handle parameters_pool;
  rtsafe_memory_pool_handle messages_pool;
  rtsafe_memory_pool_handle pending_parameter_value_changes_pool;

  lv2dynparam_parameter_created parameter_created_callback;
  lv2dynparam_parameter_destroying parameter_destroying_callback;
  lv2dynparam_parameter_value_change_context parameter_value_change_context;
};

bool
lv2dynparam_host_map_type_uri(
  struct lv2dynparam_host_parameter * parameter_ptr);

void
lv2dynparam_host_group_pending_children_count_increment(
  struct lv2dynparam_host_group * group_ptr);

void
lv2dynparam_host_group_pending_children_count_decrement(
  struct lv2dynparam_host_group * group_ptr);

void
lv2dynparam_host_parameter_free(
  struct lv2dynparam_host_instance * instance_ptr,
  struct lv2dynparam_host_parameter * parameter_ptr);

#endif /* #ifndef DYNPARAM_INTERNAL_H__86778596_B1A9_4BD7_A14A_BECBD5589468__INCLUDED */
