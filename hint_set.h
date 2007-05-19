/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of lv2dynparam libraries
 *
 *   Copyright (C) 2007 Nedko Arnaudov <nedko@arnaudov.name>
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

#ifndef HINT_SET_H__A99DFCEC_D56A_437A_AC2B_1C20D89F3B69__INCLUDED
#define HINT_SET_H__A99DFCEC_D56A_437A_AC2B_1C20D89F3B69__INCLUDED

void
lv2dynparam_hints_init_empty(
  struct lv2dynparam_hints * hints_ptr);

bool
lv2dynparam_hints_init_va_link(
  rtsafe_memory_handle memory,
  struct lv2dynparam_hints * hints_ptr,
  ...);

bool
lv2dynparam_hints_init_va_dup(
  rtsafe_memory_handle memory,
  struct lv2dynparam_hints * hints_ptr,
  ...);

bool
lv2dynparam_hints_init_copy(
  rtsafe_memory_handle memory,
  const struct lv2dynparam_hints * src_hints_ptr,
  struct lv2dynparam_hints * dest_hints_ptr);

void
lv2dynparam_hints_clear(
  struct lv2dynparam_hints * hints_ptr);

#endif /* #ifndef HINT_SET_H__A99DFCEC_D56A_437A_AC2B_1C20D89F3B69__INCLUDED */
