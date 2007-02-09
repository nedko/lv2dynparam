/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of lv2dynparam libraries
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

#ifndef HELPERS_H__E8B13390_50AB_495C_8AA3_BE4F644C13A8__INCLUDED
#define HELPERS_H__E8B13390_50AB_495C_8AA3_BE4F644C13A8__INCLUDED

char **
lv2dynparam_enum_duplicate(
  lv2dynparam_memory_handle memory,
  const char * const * values,
  unsigned int values_count);

void
lv2dynparam_enum_free(
  lv2dynparam_memory_handle memory,
  char ** values,
  unsigned int values_count);

#endif /* #ifndef HELPERS_H__E8B13390_50AB_495C_8AA3_BE4F644C13A8__INCLUDED */
