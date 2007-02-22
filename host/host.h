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

/**
 * @file host.h
 * @brief Interface to LV2 dynparam extension host helper library
 */

#ifndef DYNPARAM_H__5090F477_0BE7_439F_BF1D_F2EB78822760__INCLUDED
#define DYNPARAM_H__5090F477_0BE7_439F_BF1D_F2EB78822760__INCLUDED

#include "types.h"

/** handle to host helper library instance */
typedef void * lv2dynparam_host_instance;

/** handle to host helper library representation of parameter */
typedef void * lv2dynparam_host_parameter;

/** handle to host helper library representation of group */
typedef void * lv2dynparam_host_group;

/** handle to host helper library representation of command */
typedef void * lv2dynparam_host_command;

/**
 * Call this function to attach dynparam host helper library to particular plugin.
 * Must be called from the UI thread.
 * This function may sleep/lock.
 *
 * @param lv2descriptor LV2 descriptor of plugin to attach to.
 * @param lv2instance handle to LV2 plugin instance.
 * @param instance_ui_context user context to be associated with instance
 * @param instance_ptr Pointer to variable receiving handle to
 * host helper library instance.
 *
 * @return Success status
 * @retval TRUE - success
 * @retval FALSE - error
 */
BOOL
lv2dynparam_host_attach(
  const LV2_Descriptor * lv2descriptor,
  LV2_Handle lv2instance,
  void * instance_ui_context,
  lv2dynparam_host_instance * instance_ptr);

/**
 * Call this function to deattach dynparam host helper library from particular plugin.
 * Must be called from the UI thread.
 * This function may sleep/lock.
 *
 * @param instance Handle to instance received from lv2dynparam_host_attach()
 */
void
lv2dynparam_host_deattach(
  lv2dynparam_host_instance instance);

/**
 * Call this function to issue pending calls to plugin.
 * Must be called from from audio/midi realtime thread.
 * This function will not sleep/lock.
 *
 * @param instance Handle to instance received from lv2dynparam_host_attach()
 */
void
lv2dynparam_host_realtime_run(
  lv2dynparam_host_instance instance);

/**
 * Call this function to issue pending calls to UI.
 * Must be called from the UI thread.
 * This function may sleep/lock.
 *
 * @param instance Handle to instance received from lv2dynparam_host_attach()
 */
void
lv2dynparam_host_ui_run(
  lv2dynparam_host_instance instance);

/**
 * Call this function to change parameter as response to MIDI CC.
 * If controller is not associated with any parameter, cc will be ignored.
 * Must be called from from audio/midi realtime thread.
 * This function will not sleep/lock.
 *
 * @param controller MIDI controller
 * @param value MIDI controller value
 */
void
lv2dynparam_host_cc(
  unsigned int controller,
  unsigned int value);

/**
 * Call this function to associate MIDI CC with parameter
 * Must be called from from the UI thread.
 * This function may sleep/lock.
 *
 * @param instance Handle to instance received from lv2dynparam_host_attach()
 * @param parameter Parameter to associate CC with
 * @param controller MIDI controller
 */
void
lv2dynparam_host_cc_configure(
  lv2dynparam_host_instance instance,
  lv2dynparam_host_parameter parameter,
  unsigned int controller);

/**
 * Call this function to associate parameter from MIDI CC
 * Must be called from from the UI thread.
 * This function may sleep/lock.
 *
 * @param instance Handle to instance received from lv2dynparam_host_attach()
 * @param parameter Parameter to associate CC with
 */
void
lv2dynparam_host_cc_unconfigure(
  lv2dynparam_host_instance instance,
  lv2dynparam_host_parameter parameter);

/**
 * Call this function to trigger dynparams appear.
 * Must be called from from the UI thread.
 * This function may sleep/lock.
 *
 * @param instance Handle to instance received from lv2dynparam_host_attach()
 */
void
lv2dynparam_host_ui_on(
  lv2dynparam_host_instance instance);

/**
 * Call this function to trigger dynparams disappear.
 * Must be called from from the UI thread.
 * This function may sleep/lock.
 *
 * @param instance Handle to instance received from lv2dynparam_host_attach()
 */
void
lv2dynparam_host_ui_off(
  lv2dynparam_host_instance instance);

/**
 * Callback called from UI thread to notify UI about group appear.
 *
 * @param group_handle handle to host helper library representation of
 * group that appeared
 * @param instance_ui_context user context to be associated with instance,
 * as supplied previously to lv2dynparam_host_attach()
 * @param parent_group_ui_context user context of parent group, NULL for root group.
 * @param group_name name of group that is appearing
 * @param group_type_uri type of group that is appearing
 * @param group_ui_context Pointer to variable receiving UI context of appeared group.
 */
void
dynparam_group_appeared(
  lv2dynparam_host_group group_handle,
  void * instance_ui_context,
  void * parent_group_ui_context,
  const char * group_name,
  const char * group_type_uri,
  void ** group_ui_context);

/**
 * Callback called from UI thread to notify UI about group disappear.
 *
 * @param instance_ui_context user context to be associated with instance,
 * as supplied previously to lv2dynparam_host_attach()
 * @param parent_group_ui_context user context of parent group, NULL for root group.
 * @param group_ui_context user context of group that disappeared.
 */
void
dynparam_group_disappeared(
  void * instance_ui_context,
  void * parent_group_ui_context,
  void * group_ui_context);

/**
 * Callback called from UI thread to notify UI about command appear.
 *
 * @param command_handle handle to host helper library representation of
 * command that appeared
 * @param instance_ui_context user context to be associated with instance,
 * as supplied previously to lv2dynparam_host_attach()
 * @param group_ui_context user context of parent group, NULL for root group.
 * @param command_name name of command that is appearing
 * @param command_ui_context Pointer to variable receiving UI context of appeared command.
 */
void
dynparam_command_appeared(
  lv2dynparam_host_command command_handle,
  void * instance_ui_context,
  void * group_ui_context,
  const char * command_name,
  void ** command_ui_context);

/**
 * Callback called from UI thread to notify UI about command disappear.
 *
 * @param instance_ui_context user context to be associated with instance,
 * as supplied previously to lv2dynparam_host_attach()
 * @param parent_group_ui_context user context of parent group, NULL for root group.
 * @param command_ui_context user context of command that disappeared.
 */
void
dynparam_command_disappeared(
  void * instance_ui_context,
  void * parent_group_ui_context,
  void * command_ui_context);

/**
 * Callback called from UI thread to notify UI about boolean parameter appear.
 *
 * @param parameter_handle Handle to host helper library representation of
 * parametr that appeared
 * @param instance_ui_context User context to be associated with instance,
 * as supplied previously to lv2dynparam_host_attach()
 * @param group_ui_context User context of parent group, NULL for root group.
 * @param parameter_name Name of parameter that is appearing
 * @param value Initial value of the appearing parameter
 * @param parameter_ui_context Pointer to variable receiving UI context of appeared parameter.
 */
void
dynparam_parameter_boolean_appeared(
  lv2dynparam_host_parameter parameter_handle,
  void * instance_ui_context,
  void * group_ui_context,
  const char * parameter_name,
  BOOL value,
  void ** parameter_ui_context);

/**
 * Callback called from UI thread to notify UI about boolean parameter disappear.
 *
 * @param instance_ui_context user context to be associated with instance,
 * as supplied previously to lv2dynparam_host_attach()
 * @param parent_group_ui_context user context of parent group, NULL for root group.
 * @param parameter_ui_context user context of parameter that disappeared.
 */
void
dynparam_parameter_boolean_disappeared(
  void * instance_ui_context,
  void * parent_group_ui_context,
  void * parameter_ui_context);

/**
 * Callback called from UI thread to notify UI about float parameter appear.
 *
 * @param parameter_handle Handle to host helper library representation of
 * parametr that appeared
 * @param instance_ui_context User context to be associated with instance,
 * as supplied previously to lv2dynparam_host_attach()
 * @param group_ui_context User context of parent group, NULL for root group.
 * @param parameter_name Name of parameter that is appearing
 * @param value Initial value of the appearing parameter
 * @param min Minimum allowed value of the appearing parameter
 * @param max Maximum allowed value of the appearing parameter
 * @param parameter_ui_context Pointer to variable receiving UI context of appeared parameter.
 */
void
dynparam_parameter_float_appeared(
  lv2dynparam_host_parameter parameter_handle,
  void * instance_ui_context,
  void * group_ui_context,
  const char * parameter_name,
  float value,
  float min,
  float max,
  void ** parameter_ui_context);

/**
 * Callback called from UI thread to notify UI about float parameter disappear.
 *
 * @param instance_ui_context user context to be associated with instance,
 * as supplied previously to lv2dynparam_host_attach()
 * @param parent_group_ui_context user context of parent group, NULL for root group.
 * @param parameter_ui_context user context of parameter that disappeared.
 */
void
dynparam_parameter_float_disappeared(
  void * instance_ui_context,
  void * parent_group_ui_context,
  void * parameter_ui_context);

/**
 * Callback called from UI thread to notify UI about enumeration parameter appear.
 *
 * @param parameter_handle Handle to host helper library representation of
 * parametr that appeared
 * @param instance_ui_context User context to be associated with instance,
 * as supplied previously to lv2dynparam_host_attach()
 * @param group_ui_context User context of parent group, NULL for root group.
 * @param parameter_name Name of parameter that is appearing
 * @param selected_value Index in array pointed by the @c values_ptr_ptr parameter, specifying selected initial value
 * @param values Pointer to array of strings containing enumeration valid values.
 * @param values_count Number of strings in the array pointed by the @c values_ptr_ptr parameter
 * @param parameter_ui_context Pointer to variable receiving UI context of appeared parameter.
 */
void
dynparam_parameter_enum_appeared(
  lv2dynparam_host_parameter parameter_handle,
  void * instance_ui_context,
  void * group_ui_context,
  const char * parameter_name,
  unsigned int selected_value,
  const char * const * values,
  unsigned int values_count,
  void ** parameter_ui_context);

/**
 * Callback called from UI thread to notify UI about enumeration parameter disappear.
 *
 * @param instance_ui_context user context to be associated with instance,
 * as supplied previously to lv2dynparam_host_attach()
 * @param parent_group_ui_context user context of parent group, NULL for root group.
 * @param parameter_ui_context user context of parameter that disappeared.
 */
void
dynparam_parameter_enum_disappeared(
  void * instance_ui_context,
  void * parent_group_ui_context,
  void * parameter_ui_context);

/**
 * Callback called from UI thread to notify UI about boolean parameter value change.
 *
 * @param instance_ui_context user context to be associated with instance,
 * as supplied previously to lv2dynparam_host_attach()
 * @param parameter_ui_context user context of parameter which value has changed.
 * @param value the new value
 */
void
dynparam_parameter_boolean_changed(
  void * instance_ui_context,
  void * parameter_ui_context,
  BOOL value);

/**
 * Call this function to change boolean parameter value.
 * Must be called from the UI thread.
 * This function may sleep/lock.
 *
 * @param instance Handle to instance received from lv2dynparam_host_attach()
 * @param parameter_handle handle of parameter which value will be changed
 * @param value the new value
 */
void
dynparam_parameter_boolean_change(
  lv2dynparam_host_instance instance,
  lv2dynparam_host_parameter parameter_handle,
  BOOL value);

/**
 * Call this function to change float parameter value.
 * Must be called from the UI thread.
 * This function may sleep/lock.
 *
 * @param instance Handle to instance received from lv2dynparam_host_attach()
 * @param parameter_handle handle of parameter which value will be changed
 * @param value the new value
 */
void
dynparam_parameter_float_change(
  lv2dynparam_host_instance instance,
  lv2dynparam_host_parameter parameter_handle,
  float value);

/**
 * Call this function to change currently selected value of enumeration.
 * Must be called from the UI thread.
 * This function may sleep/lock.
 *
 * @param instance Handle to instance received from lv2dynparam_host_attach()
 * @param parameter_handle handle of parameter which value will be changed
 * @param selected_index_value index of the new value
 */
void
dynparam_parameter_enum_change(
  lv2dynparam_host_instance instance,
  lv2dynparam_host_parameter parameter_handle,
  unsigned int selected_index_value);

#endif /* #ifndef DYNPARAM_H__5090F477_0BE7_439F_BF1D_F2EB78822760__INCLUDED */
