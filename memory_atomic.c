/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   Non-sleeping memory allocation
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

#include <stdlib.h>
#include <assert.h>

#include "types.h"
#include "memory_atomic.h"
#include "list.h"
#include "log.h"

struct lv2dynparam_memory_pool
{
  size_t data_size;
  size_t min_preallocated;
  size_t max_preallocated;

  struct list_head used;
  unsigned int used_count;

  struct list_head unused;
  unsigned int unused_count;
};

#define LV2DYNPARAM_GROUPS_PREALLOCATE      1024

BOOL
lv2dynparam_memory_pool_create(
  size_t data_size,
  size_t min_preallocated,
  size_t max_preallocated,
  lv2dynparam_memory_pool_handle * pool_handle_ptr)
{
  struct lv2dynparam_memory_pool * pool_ptr;

  assert(min_preallocated <= max_preallocated);

  pool_ptr = malloc(sizeof(struct lv2dynparam_memory_pool));
  if (pool_ptr == NULL)
  {
    return FALSE;
  }

  pool_ptr->data_size = data_size;
  pool_ptr->min_preallocated = min_preallocated;
  pool_ptr->max_preallocated = max_preallocated;

  INIT_LIST_HEAD(&pool_ptr->used);
  pool_ptr->used_count = 0;

  INIT_LIST_HEAD(&pool_ptr->unused);
  pool_ptr->unused_count = 0;

  lv2dynparam_memory_pool_sleepy((lv2dynparam_memory_pool_handle)pool_ptr);
  *pool_handle_ptr = pool_ptr;

  return TRUE;
}

#define pool_ptr ((struct lv2dynparam_memory_pool *)pool_handle)

void
lv2dynparam_memory_pool_destroy(
  lv2dynparam_memory_pool_handle pool_handle)
{
  struct list_head * node_ptr;

  assert(pool_ptr->used_count == 0); /* called should deallocate all chunks prior releasing pool itself */
  assert(list_empty(&pool_ptr->used));

  while (pool_ptr->unused_count != 0)
  {
    assert(!list_empty(&pool_ptr->unused));

    node_ptr = pool_ptr->unused.next;

    list_del(node_ptr);
    pool_ptr->unused_count--;

    free(node_ptr);
  }

  assert(list_empty(&pool_ptr->unused));

  free(pool_ptr);
}

/* adjust unused list size */
void
lv2dynparam_memory_pool_sleepy(
  lv2dynparam_memory_pool_handle pool_handle)
{
  struct list_head * node_ptr;

  while (pool_ptr->unused_count < pool_ptr->min_preallocated)
  {
    node_ptr = malloc(sizeof(struct list_head) + pool_ptr->data_size);
    if (node_ptr == NULL)
    {
      return;
    }

    list_add_tail(node_ptr, &pool_ptr->unused);
    pool_ptr->unused_count++;
  }

  while (pool_ptr->unused_count > pool_ptr->max_preallocated)
  {
    assert(!list_empty(&pool_ptr->unused));

    node_ptr = pool_ptr->unused.next;

    list_del(node_ptr);
    pool_ptr->unused_count--;

    free(node_ptr);
  }
}

/* find entry in unused list, fail if it is empty */
void *
lv2dynparam_memory_pool_allocate(
  lv2dynparam_memory_pool_handle pool_handle)
{
  struct list_head * node_ptr;

  if (list_empty(&pool_ptr->unused))
  {
    return NULL;
  }

  node_ptr = pool_ptr->unused.next;
  list_del(node_ptr);
  pool_ptr->unused_count--;

  return (node_ptr + 1);
}

/* move from used to unused list */
void
lv2dynparam_memory_pool_deallocate(
  lv2dynparam_memory_pool_handle pool_handle,
  void * data)
{
  list_add_tail((struct list_head *)data - 1, &pool_ptr->unused);
  pool_ptr->used_count--;
  pool_ptr->unused_count++;
}

void *
lv2dynparam_memory_pool_allocate_sleepy(
  lv2dynparam_memory_pool_handle pool_handle)
{
  void * data;

  do
  {
    lv2dynparam_memory_pool_sleepy(pool_handle);
    data = lv2dynparam_memory_pool_allocate(pool_handle);
  }
  while (data == NULL);

  return data;
}

/* max alloc is DATA_MIN * (2 ^ POOLS_COUNT) - DATA_SUB */
#define DATA_MIN       1000
#define DATA_SUB       100      /* alloc slightly smaller chunks in hope to not allocating additional page for control data */

static struct lv2dynparam_memory_pool_generic
{
  size_t size;
  lv2dynparam_memory_pool_handle pool;
} * g_pools;

static size_t g_pools_count;

BOOL
lv2dynparam_memory_init(
  size_t max_size,
  size_t prealloc_min,
  size_t prealloc_max)
{
  size_t i;
  size_t size;

  size = DATA_MIN;
  g_pools_count = 1;

  while ((size << g_pools_count) < max_size + DATA_SUB)
  {
    g_pools_count++;

    if (g_pools_count > sizeof(size_t) * 8)
    {
      assert(0);                /* chances that caller really need such huge size are close to zero */
      return FALSE;
    }
  }

  g_pools = malloc(g_pools_count * sizeof(struct lv2dynparam_memory_pool_generic));
  if (g_pools == NULL)
  {
    return FALSE;
  }

  size = DATA_MIN;

  for (i = 0 ; i < g_pools_count ; i++)
  {
    g_pools[i].size = size - DATA_SUB;

    if (!lv2dynparam_memory_pool_create(
          g_pools[i].size,
          prealloc_min,
          prealloc_max,
          &g_pools[i].pool))
    {
      while (i > 0)
      {
        i--;
        lv2dynparam_memory_pool_destroy(g_pools[i].pool);
      }

      free(g_pools);

      return FALSE;
    }

    size = size << 1; 
  }

  return TRUE;
}

void
lv2dynparam_memory_uninit()
{
  unsigned int i;

  for (i = 0 ; i < g_pools_count ; i++)
  {
    lv2dynparam_memory_pool_destroy(g_pools[i].pool);
  }

  free(g_pools);
}

void *
lv2dynparam_memory_allocate(
  size_t size)
{
  lv2dynparam_memory_pool_handle * data_ptr;
  size_t i;

  assert(g_pools != NULL);      /* lv2dynparam_memory_init() not called? */

  /* pool handle is stored just before user data to ease deallocation */
  size += sizeof(lv2dynparam_memory_pool_handle);

  for (i = 0 ; i < g_pools_count ; i++)
  {
    if (size <= g_pools[i].size)
    {
      data_ptr = lv2dynparam_memory_pool_allocate(g_pools[i].pool);

      *data_ptr = g_pools[i].pool;

      return (data_ptr + 1);
    }
  }

  /* data size too big, increase POOLS_COUNT */
  LOG_WARNING("Data size is too big");
  return FALSE;
}

void
lv2dynparam_memory_deallocate(
  void * data)
{
  lv2dynparam_memory_pool_deallocate(
    *(lv2dynparam_memory_pool_handle *)data,
    (lv2dynparam_memory_pool_handle *)data - 1);
}
