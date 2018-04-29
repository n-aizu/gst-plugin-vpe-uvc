/* 
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) YEAR AUTHOR_NAME AUTHOR_EMAIL
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GST_ACCEL_TRANSFORM_H__
#define __GST_ACCEL_TRANSFORM_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>
#include <gst/video/gstvideopool.h>

G_BEGIN_DECLS

#define GST_TYPE_ACCEL_TRANSFORM \
  (gst_acceltransform_get_type())
#define GST_ACCEL_TRANSFORM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_ACCEL_TRANSFORM,GstAccelTransform))
#define GST_ACCEL_TRANSFORM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_ACCEL_TRANSFORM,GstAccelTransformClass))
#define GST_IS_ACCEL_TRANSFORM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_ACCEL_TRANSFORM))
#define GST_IS_ACCEL_TRANSFORM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_ACCEL_TRANSFORM))
#define GST_ACCEL_TRANSFORM_CAST(obj)  ((GstAccelTransform *)(obj))

typedef struct _GstAccelTransform GstAccelTransform;
typedef struct _GstAccelTransformClass GstAccelTransformClass;

struct _GstAccelTransform {
  GstBaseTransform element;

  GstAllocator *allocator;
  GstMemory *work_mem;
  gchar *device_name;
  gboolean negotiated;
  gboolean input_start;
  GstVideoInfo in_info;
  GstVideoInfo out_info;
  gint devfd;
  guint32 v4l2_in_size;
  guint32 v4l2_out_size;
};
struct _GstAccelTransformClass {
  GstBaseTransformClass parent_class;
};

GType gst_acceltransform_get_type (void);

G_END_DECLS

#endif /* __GST_ACCEL_TRANSFORM_H__ */
