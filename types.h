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

#ifndef TYPES_H__D2C3821C_56EC_4761_908C_F8474D26C7E3__INCLUDED
#define TYPES_H__D2C3821C_56EC_4761_908C_F8474D26C7E3__INCLUDED

#if defined(GLIB_CHECK_VERSION)
#define BOOL gboolean
#elif !defined(BOOL)
#define BOOL int
#define TRUE 1
#define FALSE 0
#endif

#endif /* #ifndef TYPES_H__D2C3821C_56EC_4761_908C_F8474D26C7E3__INCLUDED */
