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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <pthread.h>

#include "../lv2_rtmempool.h"
#include "memory_atomic.h"
#include "list.h"
//#define LOG_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

struct rtsafe_memory_pool
{
  bool atomic;
  lv2_rtsafe_memory_pool_handle lv2mempool;
  struct lv2_rtsafe_memory_pool_provider * provider_ptr;
};

#define RTSAFE_GROUPS_PREALLOCATE      1024

bool
rtsafe_memory_pool_create(
  struct lv2_rtsafe_memory_pool_provider * provider_ptr,
  const char * pool_name,
  size_t data_size,
  size_t min_preallocated,
  size_t max_preallocated,
  rtsafe_memory_pool_handle * pool_handle_ptr)
{
  struct rtsafe_memory_pool * pool_ptr;

  pool_ptr = malloc(sizeof(struct rtsafe_memory_pool));
  if (pool_ptr == NULL)
  {
    return false;
  }

  pool_ptr->atomic = false;
  pool_ptr->provider_ptr = provider_ptr;

  if (!provider_ptr->create(pool_name, data_size, min_preallocated, max_preallocated, &pool_ptr->lv2mempool))
  {
    free(pool_ptr);
    return false;
  }

  *pool_handle_ptr = pool_ptr;

  return true;
}

#define pool_ptr ((struct rtsafe_memory_pool *)pool_handle)

void
rtsafe_memory_pool_destroy(
  rtsafe_memory_pool_handle pool_handle)
{
  pool_ptr->provider_ptr->destroy(pool_ptr->lv2mempool);
  free(pool_ptr);
}

/* find entry in unused list, fail if it is empty */
void *
rtsafe_memory_pool_allocate_atomic(
  rtsafe_memory_pool_handle pool_handle)
{
  return pool_ptr->provider_ptr->allocate_atomic(pool_ptr->lv2mempool);
}

/* move from used to unused list */
void
rtsafe_memory_pool_deallocate(
  rtsafe_memory_pool_handle pool_handle,
  void * data)
{
  pool_ptr->provider_ptr->deallocate(pool_ptr->lv2mempool, data);
}

void *
rtsafe_memory_pool_allocate_sleepy(
  rtsafe_memory_pool_handle pool_handle)
{
  return pool_ptr->provider_ptr->allocate_sleepy(pool_ptr->lv2mempool);
}

void
rtsafe_memory_pool_atomic(
  rtsafe_memory_pool_handle pool_handle)
{
  pool_ptr->atomic = true;
}

void *
rtsafe_memory_pool_allocate(
  rtsafe_memory_pool_handle pool_handle)
{
  if (pool_ptr->atomic)
  {
    return rtsafe_memory_pool_allocate_atomic(pool_handle);
  }
  else
  {
    return rtsafe_memory_pool_allocate_sleepy(pool_handle);
  }
}

#undef pool_ptr

/* max alloc is DATA_MIN * (2 ^ POOLS_COUNT) - DATA_SUB */
#define DATA_MIN       1024
#define DATA_SUB       100      /* alloc slightly smaller chunks in hope to not allocating additional page for control data */

struct rtsafe_memory_pool_generic
{
  size_t size;
  rtsafe_memory_pool_handle pool;
};

struct rtsafe_memory
{
  struct rtsafe_memory_pool_generic * pools;
  size_t pools_count;
  bool atomic;
};

bool
rtsafe_memory_init(
  struct lv2_rtsafe_memory_pool_provider * provider_ptr,
  size_t max_size,
  size_t prealloc_min,
  size_t prealloc_max,
  rtsafe_memory_handle * handle_ptr)
{
  size_t i;
  size_t size;
  struct rtsafe_memory * memory_ptr;

  LOG_DEBUG("rtsafe_memory_init() called.");

  memory_ptr = malloc(sizeof(struct rtsafe_memory));
  if (memory_ptr == NULL)
  {
    goto fail;
  }

  size = DATA_MIN;
  memory_ptr->pools_count = 1;

  while ((size << memory_ptr->pools_count) < max_size + DATA_SUB)
  {
    memory_ptr->pools_count++;

    if (memory_ptr->pools_count > sizeof(size_t) * 8)
    {
      assert(0);                /* chances that caller really need such huge size are close to zero */
      goto fail_free;
    }
  }

  memory_ptr->pools = malloc(memory_ptr->pools_count * sizeof(struct rtsafe_memory_pool_generic));
  if (memory_ptr->pools == NULL)
  {
    goto fail_free;
  }

  size = DATA_MIN;

  for (i = 0 ; i < memory_ptr->pools_count ; i++)
  {
    memory_ptr->pools[i].size = size - DATA_SUB;

    if (!rtsafe_memory_pool_create(
          provider_ptr,
          "rtsafe generic",
          memory_ptr->pools[i].size,
          prealloc_min,
          prealloc_max,
          &memory_ptr->pools[i].pool))
    {
      while (i > 0)
      {
        i--;
        rtsafe_memory_pool_destroy(memory_ptr->pools[i].pool);
      }

      goto fail_free_pools;
    }

    size = size << 1; 
  }

  memory_ptr->atomic = false;

  *handle_ptr = (rtsafe_memory_handle)memory_ptr;

  return true;

fail_free_pools:
  free(memory_ptr->pools);

fail_free:
  free(memory_ptr);

fail:
  return false;
}

#define memory_ptr ((struct rtsafe_memory *)memory_handle)
void
rtsafe_memory_uninit(
  rtsafe_memory_handle memory_handle)
{
  unsigned int i;

  LOG_DEBUG("rtsafe_memory_uninit() called.");

  for (i = 0 ; i < memory_ptr->pools_count ; i++)
  {
    LOG_DEBUG("Destroying pool for size %u", (unsigned int)memory_ptr->pools[i].size);
    rtsafe_memory_pool_destroy(memory_ptr->pools[i].pool);
  }

  free(memory_ptr->pools);

  free(memory_ptr);
}

static void *
rtsafe_memory_allocate_internal(
  rtsafe_memory_handle memory_handle,
  size_t size,
  bool atomic)
{
  rtsafe_memory_pool_handle * data_ptr;
  size_t i;

  LOG_DEBUG("rtsafe_memory_allocate() called.");

  /* pool handle is stored just before user data to ease deallocation */
  size += sizeof(rtsafe_memory_pool_handle);

  for (i = 0 ; i < memory_ptr->pools_count ; i++)
  {
    if (size <= memory_ptr->pools[i].size)
    {
      LOG_DEBUG("Using chunk with size %u.", (unsigned int)memory_ptr->pools[i].size);
      data_ptr = (atomic ? rtsafe_memory_pool_allocate_atomic : rtsafe_memory_pool_allocate_sleepy)(memory_ptr->pools[i].pool);
      if (data_ptr == NULL)
      {
        LOG_DEBUG("rtsafe_memory_pool_allocate() failed.");
        return NULL;
      }

      *data_ptr = memory_ptr->pools[i].pool;

      LOG_DEBUG("rtsafe_memory_allocate() returning %p", (data_ptr + 1));
      return (data_ptr + 1);
    }
  }

  /* data size too big, increase POOLS_COUNT */
  LOG_WARNING("Data size is too big");
  return NULL;
}

void *
rtsafe_memory_allocate_atomic(
  rtsafe_memory_handle memory_handle,
  size_t size)
{
  return rtsafe_memory_allocate_internal(memory_handle, size, true);
}

void *
rtsafe_memory_allocate_sleepy(
  rtsafe_memory_handle memory_handle,
  size_t size)
{
  return rtsafe_memory_allocate_internal(memory_handle, size, false);
}

void *
rtsafe_memory_allocate(
  rtsafe_memory_handle memory_handle,
  size_t size)
{
  return rtsafe_memory_allocate_internal(memory_handle, size, memory_ptr->atomic);
}

void
rtsafe_memory_atomic(
  rtsafe_memory_handle memory_handle)
{
  memory_ptr->atomic = true;
}

void
rtsafe_memory_deallocate(
  void * data)
{
  LOG_DEBUG("rtsafe_memory_deallocate(%p) called.", data);
  rtsafe_memory_pool_deallocate(
    *((rtsafe_memory_pool_handle *)data -1),
    (rtsafe_memory_pool_handle *)data - 1);
}
