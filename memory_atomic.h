/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   Non-sleeping memory allocation
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

#ifndef MEMORY_ATOMIC_H__7B572547_304D_4597_8808_990BE4476CC3__INCLUDED
#define MEMORY_ATOMIC_H__7B572547_304D_4597_8808_990BE4476CC3__INCLUDED

typedef void * rtsafe_memory_pool_handle;

#define RTSAFE_MEMORY_POOL_NAME_MAX 128

/* will sleep */
bool
rtsafe_memory_pool_create(
  const char * pool_name,       /* pool name, for debug purposes,
                                   max RTSAFE_MEMORY_POOL_NAME_MAX chars,
                                   including terminating zero char.
                                   May be NULL */
  size_t data_size,             /* chunk size */
  size_t min_preallocated,      /* min chunks preallocated */
  size_t max_preallocated,      /* max chunks preallocated */
  bool enforce_thread_safety,   /* true - enforce thread safety (internal mutex),
                                   false - assume caller code is already thread-safe */
  rtsafe_memory_pool_handle * pool_ptr);

/* will sleep */
void
rtsafe_memory_pool_destroy(
  rtsafe_memory_pool_handle pool);

/* may sleep */
void
rtsafe_memory_pool_sleepy(
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

/* will sleep */
bool
rtsafe_memory_init(
  size_t max_size,
  size_t prealloc_min,
  size_t prealloc_max,
  bool enforce_thread_safety,   /* true - enforce thread safety (internal mutex),
                                   false - assume caller code is already thread-safe */
  rtsafe_memory_handle * handle_ptr);

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

/* may sleep */
void
rtsafe_memory_sleepy(
  rtsafe_memory_handle memory_handle);

/* will not sleep */
void
rtsafe_memory_deallocate(
  void * data);

void
rtsafe_memory_uninit(
  rtsafe_memory_handle memory_handle);

#endif /* #ifndef MEMORY_ATOMIC_H__7B572547_304D_4597_8808_990BE4476CC3__INCLUDED */
