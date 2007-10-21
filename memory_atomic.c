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

#include "memory_atomic.h"
#include "list.h"
//#define LOG_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

struct rtsafe_memory_pool
{
  char name[RTSAFE_MEMORY_POOL_NAME_MAX];
  size_t data_size;
  size_t min_preallocated;
  size_t max_preallocated;

  unsigned int used_count;
  struct list_head unused;
  struct list_head used;
  unsigned int unused_count;

  bool enforce_thread_safety;
  /* next members are initialized/used only if enforce_thread_safety is true */
  pthread_mutex_t mutex;
  unsigned int unused_count2;
  struct list_head pending;
};

#define RTSAFE_GROUPS_PREALLOCATE      1024

bool
rtsafe_memory_pool_create(
  const char * pool_name,
  size_t data_size,
  size_t min_preallocated,
  size_t max_preallocated,
  bool enforce_thread_safety,
  rtsafe_memory_pool_handle * pool_handle_ptr)
{
  int ret;
  struct rtsafe_memory_pool * pool_ptr;

  assert(min_preallocated <= max_preallocated);

  assert(pool_name == NULL || strlen(pool_name) < RTSAFE_MEMORY_POOL_NAME_MAX); 

  LOG_DEBUG("creating pool \"%s\"", pool_name);

  pool_ptr = malloc(sizeof(struct rtsafe_memory_pool));
  if (pool_ptr == NULL)
  {
    return false;
  }

  if (pool_name != NULL)
  {
    strcpy(pool_ptr->name, pool_name);
  }
  else
  {
    sprintf(pool_ptr->name, "%p", pool_ptr);
  }

  pool_ptr->data_size = data_size;
  pool_ptr->min_preallocated = min_preallocated;
  pool_ptr->max_preallocated = max_preallocated;

  INIT_LIST_HEAD(&pool_ptr->used);
  pool_ptr->used_count = 0;

  INIT_LIST_HEAD(&pool_ptr->unused);
  pool_ptr->unused_count = 0;

  pool_ptr->enforce_thread_safety = enforce_thread_safety;
  if (enforce_thread_safety)
  {
    ret = pthread_mutex_init(&pool_ptr->mutex, NULL);
    if (ret != 0)
    {
      free(pool_ptr);
      return false;
    }

    INIT_LIST_HEAD(&pool_ptr->pending);
    pool_ptr->unused_count2 = 0;
  }

  rtsafe_memory_pool_sleepy((rtsafe_memory_pool_handle)pool_ptr);
  *pool_handle_ptr = pool_ptr;

  return true;
}

#define pool_ptr ((struct rtsafe_memory_pool *)pool_handle)

void
rtsafe_memory_pool_destroy(
  rtsafe_memory_pool_handle pool_handle)
{
  int ret;
  struct list_head * node_ptr;

  LOG_DEBUG("destroying pool \"%s\"", pool_ptr->name);

  /* caller should deallocate all chunks prior releasing pool itself */
  if (pool_ptr->used_count != 0)
  {
    LOG_WARNING("Deallocating non-empty pool \"%s\", leaking %u entries:", pool_ptr->name, pool_ptr->used_count);

    list_for_each(node_ptr, &pool_ptr->used)
    {
      LOG_WARNING("    %p", node_ptr + 1);
    }

    assert(0);
  }

  while (pool_ptr->unused_count != 0)
  {
    assert(!list_empty(&pool_ptr->unused));

    node_ptr = pool_ptr->unused.next;

    list_del(node_ptr);
    pool_ptr->unused_count--;

    free(node_ptr);
  }

  assert(list_empty(&pool_ptr->unused));

  if (pool_ptr->enforce_thread_safety)
  {
    while (!list_empty(&pool_ptr->pending))
    {
      node_ptr = pool_ptr->pending.next;

      list_del(node_ptr);

      free(node_ptr);
    }

    ret = pthread_mutex_destroy(&pool_ptr->mutex);
    assert(ret == 0);
  }

  free(pool_ptr);
}

/* adjust unused list size */
void
rtsafe_memory_pool_sleepy(
  rtsafe_memory_pool_handle pool_handle)
{
  struct list_head * node_ptr;
  unsigned int count;

  LOG_DEBUG("pool \"%s\", sleepy", pool_ptr->name);

  if (pool_ptr->enforce_thread_safety)
  {
    pthread_mutex_lock(&pool_ptr->mutex);

    count = pool_ptr->unused_count2;

    assert(pool_ptr->min_preallocated < pool_ptr->max_preallocated);

    while (count < pool_ptr->min_preallocated)
    {
      node_ptr = malloc(sizeof(struct list_head) + pool_ptr->data_size);
      if (node_ptr == NULL)
      {
        break;
      }

      list_add_tail(node_ptr, &pool_ptr->pending);

      count++;
    }

    while (count > pool_ptr->max_preallocated && !list_empty(&pool_ptr->pending))
    {
      node_ptr = pool_ptr->pending.next;

      list_del(node_ptr);

      free(node_ptr);

      count--;
    }

    pthread_mutex_unlock(&pool_ptr->mutex);
  }
  else
  {
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
}

/* find entry in unused list, fail if it is empty */
void *
rtsafe_memory_pool_allocate(
  rtsafe_memory_pool_handle pool_handle)
{
  struct list_head * node_ptr;

  LOG_DEBUG("pool \"%s\", allocate (%u, %u)", pool_ptr->name, pool_ptr->used_count, pool_ptr->unused_count);

  if (list_empty(&pool_ptr->unused))
  {
    return NULL;
  }

  node_ptr = pool_ptr->unused.next;
  list_del(node_ptr);
  pool_ptr->unused_count--;
  pool_ptr->used_count++;
  list_add_tail(node_ptr, &pool_ptr->used);

  if (pool_ptr->enforce_thread_safety &&
      pthread_mutex_trylock(&pool_ptr->mutex) == 0)
  {
    while (pool_ptr->unused_count < pool_ptr->min_preallocated && !list_empty(&pool_ptr->pending))
    {
      node_ptr = pool_ptr->pending.next;

      list_del(node_ptr);
      list_add_tail(node_ptr, &pool_ptr->unused);
      pool_ptr->unused_count++;
    }

    pool_ptr->unused_count2 = pool_ptr->unused_count;

    pthread_mutex_unlock(&pool_ptr->mutex);
  }

  LOG_DEBUG("pool \"%s\", allocated %p (%u)", pool_ptr->name, node_ptr + 1, pool_ptr->used_count);
  return (node_ptr + 1);
}

/* move from used to unused list */
void
rtsafe_memory_pool_deallocate(
  rtsafe_memory_pool_handle pool_handle,
  void * data)
{
  struct list_head * node_ptr;

  LOG_DEBUG("pool \"%s\", deallocate %p (%u)", pool_ptr->name, (struct list_head *)data - 1, pool_ptr->used_count);

  list_del((struct list_head *)data - 1);
  list_add_tail((struct list_head *)data - 1, &pool_ptr->unused);
  pool_ptr->used_count--;
  pool_ptr->unused_count++;

  if (pool_ptr->enforce_thread_safety &&
      pthread_mutex_trylock(&pool_ptr->mutex) == 0)
  {
    while (pool_ptr->unused_count > pool_ptr->max_preallocated)
    {
      assert(!list_empty(&pool_ptr->unused));

      node_ptr = pool_ptr->unused.next;

      list_del(node_ptr);
      list_add_tail(node_ptr, &pool_ptr->pending);
      pool_ptr->unused_count--;
    }

    pool_ptr->unused_count2 = pool_ptr->unused_count;

    pthread_mutex_unlock(&pool_ptr->mutex);
  }
}

void *
rtsafe_memory_pool_allocate_sleepy(
  rtsafe_memory_pool_handle pool_handle)
{
  void * data;

  LOG_DEBUG("pool \"%s\", allocate sleepy", pool_ptr->name);

  do
  {
    rtsafe_memory_pool_sleepy(pool_handle);
    data = rtsafe_memory_pool_allocate(pool_handle);
  }
  while (data == NULL);

  return data;
}

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
};

bool
rtsafe_memory_init(
  size_t max_size,
  size_t prealloc_min,
  size_t prealloc_max,
  bool enforce_thread_safety,
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
          "rtsafe generic",
          memory_ptr->pools[i].size,
          prealloc_min,
          prealloc_max,
          enforce_thread_safety,
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

  *handle_ptr = (rtsafe_memory_handle)memory_ptr;

  return true;

fail_free_pools:
  free(memory_ptr->pools);

fail_free:
  free(memory_ptr);

fail:
  return false;
}

#define memory_ptr ((struct rtsafe_memory *)handle_ptr)
void
rtsafe_memory_uninit(
  rtsafe_memory_handle handle_ptr)
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

void *
rtsafe_memory_allocate(
  rtsafe_memory_handle handle_ptr,
  size_t size)
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
      data_ptr = rtsafe_memory_pool_allocate(memory_ptr->pools[i].pool);
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

void
rtsafe_memory_sleepy(
  rtsafe_memory_handle handle_ptr)
{
  unsigned int i;

  for (i = 0 ; i < memory_ptr->pools_count ; i++)
  {
    rtsafe_memory_pool_sleepy(memory_ptr->pools[i].pool);
  }
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
