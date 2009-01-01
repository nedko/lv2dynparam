/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   Non-sleeping memory allocation
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

#ifndef MEMORY_ATOMIC_H__7B572547_304D_4597_8808_990BE4476CC3__INCLUDED
#define MEMORY_ATOMIC_H__7B572547_304D_4597_8808_990BE4476CC3__INCLUDED

typedef void * rtsafe_memory_pool_handle;

#if defined(LV2_RTSAFE_MEMORY_POOL_NAME_MAX)
/* will sleep */
bool
rtsafe_memory_pool_create(
  struct lv2_rtsafe_memory_pool_provider * provider_ptr,
  const char * pool_name,
  size_t data_size,
  size_t min_preallocated,
  size_t max_preallocated,
  rtsafe_memory_pool_handle * pool_ptr);
#endif

/* will sleep */
void
rtsafe_memory_pool_destroy(
  rtsafe_memory_pool_handle pool);

/* will not sleep, returns NULL if no memory is available */
void *
rtsafe_memory_pool_allocate_atomic(
  rtsafe_memory_pool_handle pool);

/* may sleep, will not fail under normal conditions */
void *
rtsafe_memory_pool_allocate_sleepy(
  rtsafe_memory_pool_handle pool);

/* may or may not sleep, depending of whether atomic mode is enabled */
void *
rtsafe_memory_pool_allocate(
  rtsafe_memory_pool_handle pool);

/* switch to atomic mode */
void
rtsafe_memory_pool_atomic(
  rtsafe_memory_pool_handle pool);

/* will not sleep */
void
rtsafe_memory_pool_deallocate(
  rtsafe_memory_pool_handle pool,
  void * data);

typedef void * rtsafe_memory_handle;

#if defined(LV2_RTSAFE_MEMORY_POOL_NAME_MAX)
/* will sleep */
bool
rtsafe_memory_init(
  struct lv2_rtsafe_memory_pool_provider * provider_ptr,
  size_t max_size,
  size_t prealloc_min,
  size_t prealloc_max,
  rtsafe_memory_handle * handle_ptr);
#endif

/* will not sleep, returns NULL if no memory is available */
void *
rtsafe_memory_allocate_atomic(
  rtsafe_memory_handle memory_handle,
  size_t size);

/* may sleep, will not fail under normal conditions */
void *
rtsafe_memory_allocate_sleepy(
  rtsafe_memory_handle memory_handle,
  size_t size);

/* may or may not sleep, depending of whether atomic mode is enabled */
void *
rtsafe_memory_allocate(
  rtsafe_memory_handle memory_handle,
  size_t size);

/* switch to atomic mode */
void
rtsafe_memory_atomic(
  rtsafe_memory_handle memory_handle);

/* will not sleep */
void
rtsafe_memory_deallocate(
  void * data);

void
rtsafe_memory_uninit(
  rtsafe_memory_handle memory_handle);

#endif /* #ifndef MEMORY_ATOMIC_H__7B572547_304D_4597_8808_990BE4476CC3__INCLUDED */
