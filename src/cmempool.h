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

#ifndef __GST_CMEMPOOL_H__
#define __GST_CMEMPOOL_H__

#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>
#include <gst/video/gstvideopool.h>

G_BEGIN_DECLS

typedef struct _GstCMemMemory GstCMemMemory;

typedef struct _GstCMemBufferPool GstCMemBufferPool;
typedef struct _GstCMemBufferPoolClass GstCMemBufferPoolClass;


/**
 * GstCMemMemory:
 * @width: the width in pixels of CMem @cmem
 * @height: the height in pixels of CMem @cmem
 * @size: the size in bytes of CMem @cmem
 *
 * Subclass of #GstMemory containing additional information about an CMem.
 */
struct _GstCMemMemory
{
  GstMemory parent;
  guint8 *data;
  int fd;
  uint index;
};


/* buffer pool functions */
#define GST_TYPE_CMEM_BUFFER_POOL      (gst_cmem_buffer_pool_get_type())
#define GST_IS_CMEM_BUFFER_POOL(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_CMEM_BUFFER_POOL))
#define GST_CMEM_BUFFER_POOL(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_CMEM_BUFFER_POOL, GstCMemBufferPool))
#define GST_CMEM_BUFFER_POOL_CAST(obj) ((GstCMemBufferPool*)(obj))

struct _GstCMemBufferPool
{
  GstBufferPool bufferpool;

  GstAllocator *allocator;

  GstCaps *caps;
  GstVideoInfo info;
  guint32 fourcc;
};

struct _GstCMemBufferPoolClass
{
  GstBufferPoolClass parent_class;
};

GType gst_cmem_buffer_pool_get_type (void);

GstBufferPool * gst_cmem_buffer_pool_new     (void);


typedef GstAllocator GstCMemMemoryAllocator;
typedef GstAllocatorClass GstCMemMemoryAllocatorClass;

GType gst_cmem_memory_allocator_get_type (void);

#define GST_CMEM_ALLOCATOR_NAME "cmem_allocator"
#define GST_TYPE_CMEM_MEMORY_ALLOCATOR   (gst_cmem_memory_allocator_get_type())
#define GST_IS_CMEM_MEMORY_ALLOCATOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_CMEM_MEMORY_ALLOCATOR))


/* XXX: roughly determined value */
#define CMEM_POOL_MIN_BUF_NUM 3
#define CMEM_POOL_MAX_BUF_NUM 8

G_END_DECLS

#endif /* __GST_CMEMPOOL_H__ */
