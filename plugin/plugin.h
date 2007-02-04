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

#ifndef DYNPARAM_H__84DA2DA3_61BD_45AC_B202_6A08F27D56F5__INCLUDED
#define DYNPARAM_H__84DA2DA3_61BD_45AC_B202_6A08F27D56F5__INCLUDED

#include "types.h"

void *
get_lv2dynparam_plugin_extension_data(void);

typedef void * lv2dynparam_plugin_instance;
typedef void * lv2dynparam_plugin_parameter;
typedef void * lv2dynparam_plugin_group;

BOOL
lv2dynparam_plugin_instantiate(
  LV2_Handle instance,
  const char * root_group_name,
  lv2dynparam_plugin_instance * instance_ptr);

void
lv2dynparam_plugin_cleanup(
  lv2dynparam_plugin_instance instance_ptr);

typedef BOOL
(*lv2dynparam_plugin_param_boolean_changed)(
  void * context,
  BOOL value);

typedef BOOL
(*lv2dynparam_plugin_param_float_changed)(
  void * context,
  float value);

typedef BOOL
(*lv2dynparam_plugin_param_enum_changed)(
  void * context,
  const char * value,
  unsigned int value_index);

BOOL
lv2dynparam_plugin_group_add(
  lv2dynparam_plugin_instance instance,
  lv2dynparam_plugin_group parent_group,
  const char * name,
  const char * type_uri,
  lv2dynparam_plugin_group * group_ptr);

BOOL
lv2dynparam_plugin_param_boolean_add(
  lv2dynparam_plugin_instance instance,
  lv2dynparam_plugin_group group,
  const char * name,
  int value,
  lv2dynparam_plugin_param_boolean_changed callback,
  void * callback_context,
  lv2dynparam_plugin_parameter * param_ptr);

BOOL
lv2dynparam_plugin_param_float_add(
  lv2dynparam_plugin_instance instance,
  lv2dynparam_plugin_group group,
  const char * name,
  float value,
  float min,
  float max,
  lv2dynparam_plugin_param_float_changed callback,
  void * callback_context,
  lv2dynparam_plugin_parameter * param_ptr);

BOOL
lv2dynparam_plugin_param_enum_add(
  lv2dynparam_plugin_instance instance,
  lv2dynparam_plugin_group group,
  const char * name,
  const char ** values_ptr_ptr,
  unsigned int values_count,
  unsigned int initial_value_index,
  lv2dynparam_plugin_param_enum_changed callback,
  void * callback_context,
  lv2dynparam_plugin_parameter * param_ptr);

/* called by plugin when it decides to remove parameter */
BOOL
lv2dynparam_plugin_param_remove(
  lv2dynparam_plugin_instance instance,
  lv2dynparam_plugin_parameter param);

/* called by plugin when it decides to change bool parameter value */
BOOL
lv2dynparam_plugin_param_boolean_change(
  lv2dynparam_plugin_instance instance,
  lv2dynparam_plugin_parameter param,
  BOOL value);

#endif /* #ifndef DYNPARAM_H__84DA2DA3_61BD_45AC_B202_6A08F27D56F5__INCLUDED */
