/* GStreamer
 * Copyright (C) <2005> Julien Moutte <julien@moutte.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include "cmempool.h"
#include "cmem_buf.h"

/* Debugging category */
#include <gst/gstinfo.h>

GST_DEBUG_CATEGORY_STATIC (gst_cmem_pool_debug);
#define GST_CAT_DEFAULT gst_cmem_pool_debug

static gint mem_counter;

static GstMemory *
gst_cmem_memory_alloc (GstAllocator * allocator, gsize size,
    GstAllocationParams * params)
{
  GstCMemMemory *mem;

  mem = g_slice_new (GstCMemMemory);

  /* XXX: test */
#if 0
  mem->data = g_malloc (size);
  mem->fd = 0xdeadbeef;
#else
  mem->fd = alloc_cmem_buffer (size, 1, (void **)&mem->data);
  if (mem->fd < 0) {
    GST_WARNING_OBJECT (allocator, "cmem alloc failed(%d)", mem->fd);
    return NULL;
  }
#endif

  mem->index = g_atomic_int_add (&mem_counter, 1);

  gst_memory_init (GST_MEMORY_CAST (mem), params->flags, allocator, NULL,
      size + params->prefix + params->padding, params->align, params->prefix, size);

  return GST_MEMORY_CAST (mem);

}

static void
gst_cmem_memory_free (GstAllocator * allocator, GstMemory * gmem)
{
  GstCMemMemory *mem = (GstCMemMemory *) gmem;

  /* XXX: test */
#if 0
  g_free (mem->data);
#else
  free_cmem_buffer (mem->data);
#endif
  g_slice_free (GstCMemMemory, mem);

  g_atomic_int_add (&mem_counter, -1);
}

static gpointer
gst_cmem_memory_map (GstCMemMemory * mem, gsize maxsize, GstMapFlags flags)
{
  return mem->data + mem->parent.offset;
}

static gboolean
gst_cmem_memory_unmap (GstCMemMemory * mem)
{
  return TRUE;
}


G_DEFINE_TYPE (GstCMemMemoryAllocator, gst_cmem_memory_allocator,
    GST_TYPE_ALLOCATOR);

static void
gst_cmem_memory_allocator_class_init (GstCMemMemoryAllocatorClass * klass)
{
  GstAllocatorClass *allocator_class;

  allocator_class = (GstAllocatorClass *) klass;

  allocator_class->alloc = gst_cmem_memory_alloc;
  allocator_class->free = gst_cmem_memory_free;

  g_atomic_int_set (&mem_counter, 0);
  init_cmem();
}

static void
gst_cmem_memory_allocator_init (GstCMemMemoryAllocator * allocator)
{
  GstAllocator *alloc = GST_ALLOCATOR_CAST (allocator);

  alloc->mem_type = GST_CMEM_ALLOCATOR_NAME;
  alloc->mem_map = (GstMemoryMapFunction) gst_cmem_memory_map;
  alloc->mem_unmap = (GstMemoryUnmapFunction) gst_cmem_memory_unmap;

  GST_OBJECT_FLAG_SET (allocator, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);
}


/* bufferpool */
static void gst_cmem_buffer_pool_finalize (GObject * object);

#define gst_cmem_buffer_pool_parent_class parent_class
G_DEFINE_TYPE (GstCMemBufferPool, gst_cmem_buffer_pool,
    GST_TYPE_BUFFER_POOL);


static gboolean
cmem_buffer_pool_set_config (GstBufferPool * pool, GstStructure * config)
{
  GstCMemBufferPool *cpool = GST_CMEM_BUFFER_POOL_CAST (pool);
  GstVideoInfo info;
  GstCaps *caps;
  GstStructure *structure;
  guint size, min_buffers, max_buffers;
  const gchar *fmt;

  if (!gst_buffer_pool_config_get_params (config, &caps, &size, &min_buffers,
          &max_buffers))
    goto wrong_config;

  if (caps == NULL)
    goto no_caps;

  /* now parse the caps from the config */
  if (!gst_video_info_from_caps (&info, caps))
    goto wrong_caps;

  structure = gst_caps_get_structure (caps, 0);
  fmt = gst_structure_get_string (structure, "format");
  if (fmt == NULL)
    goto wrong_caps;

  cpool->fourcc = GST_STR_FOURCC (fmt);

  GST_INFO_OBJECT (pool, "%dx%d, caps %" GST_PTR_FORMAT, info.width, info.height,
      caps);

  /* keep track of the width and height and caps */
  if (cpool->caps)
    gst_caps_unref (cpool->caps);
  cpool->caps = gst_caps_ref (caps);

  cpool->info = info;

  gst_buffer_pool_config_set_params (config, caps, info.size, min_buffers,
      max_buffers);

  return GST_BUFFER_POOL_CLASS (parent_class)->set_config (pool, config);

  /* ERRORS */
wrong_config:
  {
    GST_WARNING_OBJECT (pool, "invalid config");
    return FALSE;
  }
no_caps:
  {
    GST_INFO_OBJECT (pool, "no caps in config");
    return FALSE;
  }
wrong_caps:
  {
    GST_WARNING_OBJECT (pool,
        "failed getting geometry from caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }
}

static GstFlowReturn
cmem_buffer_pool_alloc (GstBufferPool * pool, GstBuffer ** buffer,
    GstBufferPoolAcquireParams * params)
{
  GstCMemBufferPool *cpool = GST_CMEM_BUFFER_POOL_CAST (pool);
  GstBuffer *newbuf;
  GstMemory *mem;

  mem = gst_allocator_alloc (cpool->allocator, cpool->info.size, NULL);
  if (mem == NULL)
    goto no_mem;

  newbuf = gst_buffer_new ();
  gst_buffer_append_memory (newbuf, mem);
  *buffer = newbuf;

  return GST_FLOW_OK;

no_mem:
  {
    GST_WARNING_OBJECT (pool, "failed allocate memory");
    return FALSE;
  }
}

GstBufferPool *
gst_cmem_buffer_pool_new (void)
{
  GstBufferPool *pool;
  GstCMemBufferPool *cpool;
  GstStructure *config;

  cpool = g_object_new (GST_TYPE_CMEM_BUFFER_POOL, NULL);
  cpool->allocator = g_object_new (GST_TYPE_CMEM_MEMORY_ALLOCATOR, NULL);

  pool = GST_BUFFER_POOL_CAST (cpool);
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_allocator (config, cpool->allocator, NULL);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  if (gst_buffer_pool_set_config (pool, config))
      goto config_failed;

  return pool;

config_failed:
  {
    GST_WARNING_OBJECT (cpool, "failed setting config");
    gst_object_unref (cpool);
    return NULL;
  }
}

static void
gst_cmem_buffer_pool_class_init (GstCMemBufferPoolClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBufferPoolClass *gstbufferpool_class = (GstBufferPoolClass *) klass;

  gobject_class->finalize = gst_cmem_buffer_pool_finalize;

  gstbufferpool_class->set_config = cmem_buffer_pool_set_config;
  gstbufferpool_class->alloc_buffer = cmem_buffer_pool_alloc;

  GST_DEBUG_CATEGORY_INIT (gst_cmem_pool_debug, "cmempool", 0,
      "cmempool");
}

static void
gst_cmem_buffer_pool_init (GstCMemBufferPool * pool)
{
  /* nothing to do here */
}

static void
gst_cmem_buffer_pool_finalize (GObject * object)
{
  GstCMemBufferPool *pool = GST_CMEM_BUFFER_POOL_CAST (object);

  GST_LOG_OBJECT (pool, "finalize CMem buffer pool %p", pool);

  if (pool->caps)
    gst_caps_unref (pool->caps);
  gst_object_unref (pool->allocator);

  G_OBJECT_CLASS (gst_cmem_buffer_pool_parent_class)->finalize (object);
}
