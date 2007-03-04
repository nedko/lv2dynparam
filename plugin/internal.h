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

#ifndef DYNPARAM_INTERNAL_H__1A466106_9E02_4FA2_9D30_888795C93BC9__INCLUDED
#define DYNPARAM_INTERNAL_H__1A466106_9E02_4FA2_9D30_888795C93BC9__INCLUDED

#define LV2DYNPARAM_GROUP_TYPE_GENERIC   0
#define LV2DYNPARAM_GROUP_TYPE_ADSR      1

#define LV2DYNPARAM_PARAMETER_TYPE_COMMAND   0
#define LV2DYNPARAM_PARAMETER_TYPE_FLOAT     1
#define LV2DYNPARAM_PARAMETER_TYPE_INT       2
#define LV2DYNPARAM_PARAMETER_TYPE_NOTE      3
#define LV2DYNPARAM_PARAMETER_TYPE_STRING    4
#define LV2DYNPARAM_PARAMETER_TYPE_FILENAME  5
#define LV2DYNPARAM_PARAMETER_TYPE_BOOLEAN   6
#define LV2DYNPARAM_PARAMETER_TYPE_ENUM      7

#define LV2DYNPARAM_PENDING_NOTHING    0 /* nothing pending */
#define LV2DYNPARAM_PENDING_APPEAR     1 /* pending appear */
#define LV2DYNPARAM_PENDING_DISAPPEAR  2 /* pending disappear */
#define LV2DYNPARAM_PENDING_CHANGE     3 /* pending change */

struct lv2dynparam_plugin_group
{
  struct list_head siblings;    /* siblings in parent group child_parameters */

  struct lv2dynparam_plugin_group * group_ptr; /* parent group */

  struct lv2dynparam_hints hints;
  char name[LV2DYNPARAM_MAX_STRING_SIZE];
  char type_uri[LV2DYNPARAM_MAX_STRING_SIZE];
  struct list_head child_groups;
  struct list_head child_parameters;

  unsigned int pending;         /* One of LV2DYNPARAM_PENDING_XXX */

  void * host_context;
};

struct lv2dynparam_plugin_parameter
{
  struct list_head siblings;    /* siblings in parent group child_parameters */

  struct lv2dynparam_plugin_group * group_ptr; /* parent group */

  struct lv2dynparam_hints hints;
  unsigned int type;
  char name[LV2DYNPARAM_MAX_STRING_SIZE];
  union
  {
    struct
    {
      float value;
      float min;
      float max;
    } fpoint;
    struct
    {
      int value;
      int min;
      int max;
    } integer;
    struct
    {
      unsigned char value;
      unsigned char min;
      unsigned char max;
    } note;
/*     char string[??]; */
/*     char filename[??]; */
    struct
    {
      char ** values;
      unsigned int values_count;
      unsigned int selected_value;
    } enumeration;
    unsigned char boolean;
    struct
    {
      int (*callback)(void * context);
      void * context;
    } command;
  } data;
  union
  {
    lv2dynparam_plugin_param_boolean_changed boolean;
    lv2dynparam_plugin_param_float_changed fpoint;
    lv2dynparam_plugin_param_enum_changed enumeration;
  } plugin_callback;
  void * plugin_callback_context;

  unsigned int pending;         /* One of LV2DYNPARAM_PENDING_XXX */

  void * host_context;
};

struct lv2dynparam_plugin_instance
{
  struct list_head siblings;
  lv2dynparam_memory_handle memory;
  LV2_Handle lv2instance;
  struct lv2dynparam_plugin_group root_group;
  struct lv2dynparam_host_callbacks * host_callbacks;
  void * host_context;

  unsigned int pending;

  lv2dynparam_memory_pool_handle groups_pool;
  lv2dynparam_memory_pool_handle parameters_pool;
};

unsigned char
lv2dynparam_plugin_host_attach(
  LV2_Handle instance,
  struct lv2dynparam_host_callbacks * host_callbacks,
  void * instance_host_context);

BOOL
lv2dynparam_plugin_group_init(
  struct lv2dynparam_plugin_instance * instance_ptr,
  struct lv2dynparam_plugin_group * group_ptr,
  struct lv2dynparam_plugin_group * parent_group_ptr,
  const char * name,
  const char * type_uri);

void
lv2dynparam_plugin_group_clean(
  struct lv2dynparam_plugin_instance * instance_ptr,
  struct lv2dynparam_plugin_group * group_ptr);

void
lv2dynparam_plugin_group_free(
  struct lv2dynparam_plugin_instance * instance_ptr,
  struct lv2dynparam_plugin_group * group_ptr);

void
lv2dynparam_plugin_group_notify(
  struct lv2dynparam_plugin_instance * instance_ptr,
  struct lv2dynparam_plugin_group * group_ptr);

void
lv2dynparam_plugin_group_get_type_uri(
  lv2dynparam_group_handle group,
  char * buffer);

void
lv2dynparam_plugin_group_get_name(
  lv2dynparam_group_handle group,
  char * buffer);

void
lv2dynparam_plugin_parameter_free(
  struct lv2dynparam_plugin_instance * instance_ptr,
  struct lv2dynparam_plugin_parameter * param_ptr);

void
lv2dynparam_plugin_param_notify(
  struct lv2dynparam_plugin_instance * instance_ptr,
  struct lv2dynparam_plugin_parameter * param_ptr);

void
lv2dynparam_plugin_parameter_get_type_uri(
  lv2dynparam_parameter_handle parameter,
  char * buffer);

void
lv2dynparam_plugin_parameter_get_name(
  lv2dynparam_parameter_handle parameter,
  char * buffer);

void
lv2dynparam_plugin_parameter_get_value(
  lv2dynparam_parameter_handle parameter,
  void ** value_buffer);

void
lv2dynparam_plugin_parameter_get_range(
  lv2dynparam_parameter_handle parameter,
  void ** value_min_buffer,
  void ** value_max_buffer);

unsigned char
lv2dynparam_plugin_parameter_change(
  lv2dynparam_parameter_handle parameter);

#endif /* #ifndef DYNPARAM_INTERNAL_H__1A466106_9E02_4FA2_9D30_888795C93BC9__INCLUDED */
