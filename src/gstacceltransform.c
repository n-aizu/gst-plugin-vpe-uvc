/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2003> David Schleef <ds@schleef.org>
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

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/videodev2.h>

#include <gst/gst.h>

#include "gstacceltransform.h"
#include "cmempool.h"
#include "cmem_buf.h"
#include "v4l2_m2m.h"

GST_DEBUG_CATEGORY_STATIC (gst_acceltransform_debug);
#define GST_CAT_DEFAULT gst_acceltransform_debug

enum
{
  PROP_0,
  PROP_DEVNAME,
};

#define DEFAULT_DEVICE_NAME "/dev/v4l/by-path/platform-489d0000.vpe-video-index0"

#define OUTPUT_BUF_QUEUE_NUM 2

typedef struct {
  void *buf;
  int fd;
} cmem_buf;

static cmem_buf out_cbuf[OUTPUT_BUF_QUEUE_NUM];


static gboolean
get_v4l2_fmt (const GstVideoInfo *vinfo, uint32_t *fourcc, enum v4l2_colorspace *clrspc)
{
  gboolean ret = TRUE;
  GstVideoFormat gfmt = GST_VIDEO_INFO_FORMAT(vinfo);

  if (GST_VIDEO_INFO_IS_YUV(vinfo)) {
    if (gfmt == GST_VIDEO_FORMAT_YUY2)
      *fourcc = V4L2_PIX_FMT_YUYV;
    else
      *fourcc = gst_video_format_to_fourcc(gfmt);

    *clrspc = V4L2_COLORSPACE_SMPTE170M;
  }
  else {
    switch (gfmt) {
    case GST_VIDEO_FORMAT_RGB:
      *fourcc = V4L2_PIX_FMT_RGB24;
      break;

    case GST_VIDEO_FORMAT_BGR:
      *fourcc = V4L2_PIX_FMT_BGR24;
      break;

    case GST_VIDEO_FORMAT_ARGB:
    case GST_VIDEO_FORMAT_xRGB:
      *fourcc = V4L2_PIX_FMT_RGB32;
      break;

    case GST_VIDEO_FORMAT_ABGR:
    case GST_VIDEO_FORMAT_xBGR:
      *fourcc = V4L2_PIX_FMT_BGR32;
      break;

    default:
      ret = FALSE;
      goto end;
    }

    *clrspc = V4L2_COLORSPACE_SRGB;
  }

end:
  return ret;
}


static gboolean
init_device (GstAccelTransform *atrans)
{
  gint fd, ret, i;
  gint width, height, max_num;
  uint32_t fourcc = 0;
  enum v4l2_colorspace clrspc = 0;
  uint32_t *sizeimage;
  const GstVideoInfo *vinfo;
  const gchar *devname = DEFAULT_DEVICE_NAME;

  if (atrans->device_name)
    devname = atrans->device_name;

  fd = open(devname, O_RDWR);
  if (fd < 0) {
    GST_ERROR_OBJECT (atrans, "open %s failed: %s", devname, strerror(errno));
    goto err_end;
  }

  sizeimage = &atrans->v4l2_in_size;
  vinfo = &atrans->in_info;
  max_num = CMEM_POOL_MAX_BUF_NUM; /* all input buffers are allocated with cmem allocator */

  for (i = 0; i < 2; i++) {
    /* input/output buffer settings */

    ret = get_v4l2_fmt (vinfo, &fourcc, &clrspc);
    if (ret < 0) {
      GST_ERROR_OBJECT (atrans, "color format is incompatible(index:%d)", i);
      goto err_close;
    }

    width = GST_VIDEO_INFO_WIDTH (vinfo);
    height = GST_VIDEO_INFO_HEIGHT (vinfo);

    ret = v4l2_request_buffer (fd, width, height, fourcc, clrspc, max_num, (i == 0), sizeimage);
    if (ret < 0) {
      GST_ERROR_OBJECT (atrans, "buffer initialize failed(input:%s)", (i == 0 ? "yes" : "no"));
      goto err_close;
    }

    /* XXX: */
	/* assert (GST_VIDEO_INFO_SIZE (vinfo) == *sizeimage); */

    sizeimage = &atrans->v4l2_out_size;
    vinfo = &atrans->out_info;
    max_num = OUTPUT_BUF_QUEUE_NUM;
  }

  atrans->devfd = fd;
  return TRUE;

err_close:
  close (fd);
err_end:
  return FALSE;
}


static gboolean
wrap_queue_buffer (GstAccelTransform *atrans,
    gint index, gint dma_fd, void *buf, gsize size, gboolean is_input)
{
  gboolean bret = TRUE;
  gint ret, op;
  uint32_t sizeimage;

  if (is_input) {
    /* flush cache for device read after cpu write */
    op = CMEM_CACHE_FLUSH;
    sizeimage = atrans->v4l2_in_size;
  }
  else {
    /* invalidate cache for next cpu read after device write */
    op = CMEM_CACHE_INVALIDATE;
    sizeimage = atrans->v4l2_out_size;
  }

  ret = cmem_do_cache_operation (buf, size, op);
  if (ret < 0)
    GST_WARNING_OBJECT (atrans, "cache operation(%d) failed", op);

  /* queue buffer */
  ret = v4l2_queue_buffer (atrans->devfd, index, dma_fd, sizeimage, (int)is_input);
  if (ret < 0) {
    GST_ERROR_OBJECT (atrans, "queue buffer failed(dma_fd:%d/input:%d)", dma_fd, (int)is_input);
    bret = FALSE;
  }

  return bret;
}


static gboolean setup_device (GstAccelTransform *atrans)
{
  gint ret, fd, i;
  gboolean bret;
  gsize size;
  void *buf;
  GstMemory *mem;

  if (!init_device (atrans))
    return FALSE;

  /* setup input work buffer */
  size = atrans->in_info.size;
  atrans->allocator = g_object_new (GST_TYPE_CMEM_MEMORY_ALLOCATOR, NULL);
  mem = gst_allocator_alloc (atrans->allocator, size, NULL);
  if (mem == NULL) {
    GST_ERROR_OBJECT (atrans, "gst_allocator_alloc failed");
    goto failed;
  }

  atrans->work_mem = mem;

  /* setup output buffer */
  size = atrans->out_info.size;
  for (i = 0; i < OUTPUT_BUF_QUEUE_NUM; i++) {
    fd = alloc_cmem_buffer (size, 1, &buf);
    if (fd < 0) {
      GST_ERROR_OBJECT (atrans, "alloc cmem failed(ret:%d)", fd);
      goto failed;
    }

    /* queue output buffer */
    bret = wrap_queue_buffer (atrans, i, fd, buf, size, FALSE);
    if (!bret) {
      GST_ERROR_OBJECT (atrans, "queue buffer failed");
      goto failed;
    }

    out_cbuf[i].fd = fd;
    out_cbuf[i].buf = buf;
  }

  ret = v4l2_stream_on (atrans->devfd, 0);
  if (ret < 0) {
    GST_ERROR_OBJECT (atrans, "output stream start failed");
    goto failed;
  }

  return TRUE;

failed:
  /* XXX: omit cleanup */
  return FALSE;
}


static void cleanup_device (GstAccelTransform *atrans)
{
  int i;

  if (atrans->input_start) {
    (void)v4l2_stream_off (atrans->devfd, 1);
    atrans->input_start = FALSE;
  }

  (void)v4l2_stream_off (atrans->devfd, 0);

  for (i = 0; i < OUTPUT_BUF_QUEUE_NUM; i++) {
    free_cmem_buffer (out_cbuf[i].buf);
  }

  close (atrans->devfd);
  atrans->devfd = -1;
}


static GstFlowReturn
hwtransform (GstAccelTransform *atrans, const void *in, void *out, gboolean input_is_cmem)
{
  gint ret, index;
  gboolean bret;
  const GstCMemMemory *cmem;

  if (input_is_cmem) {
    cmem = (GstCMemMemory *)in;
  }
  else {
    cmem = (GstCMemMemory *)atrans->work_mem;
    memcpy (cmem->data, in, atrans->in_info.size);
  }

  /* queue input buffer */
  bret = wrap_queue_buffer (atrans,
      cmem->index, cmem->fd, cmem->data, GST_MEMORY_CAST (cmem)->size, TRUE);
  if (!bret) {
    GST_ERROR_OBJECT (atrans, "queue input buffer failed");
    goto failed;
  }

  if (G_UNLIKELY (!atrans->input_start)) {
    ret = v4l2_stream_on (atrans->devfd, 1);
    if (ret < 0) {
      GST_ERROR_OBJECT (atrans, "input stream start failed");
      goto failed;
    }

    atrans->input_start = TRUE;
  }

  /* wait for output */
  ret = v4l2_dequeue_buffer (atrans->devfd, 0);
  if (ret < 0) {
    GST_ERROR_OBJECT (atrans, "dequeue buffer failed");
    goto failed;
  }

  /* XXX: */
  /* assert (ret < OUTPUT_BUF_QUEUE_NUM); */

  index = ret;
  memcpy (out, out_cbuf[index].buf, atrans->out_info.size);

  /* dequeue input buffer */
  ret = v4l2_dequeue_buffer (atrans->devfd, 1);
  if (ret < 0) {
    GST_ERROR_OBJECT (atrans, "dequeue buffer failed");
    goto failed;
  }

  /* XXX: */
  /* assert (ret == cmem->index); */

  /* queue output buffer */
  bret = wrap_queue_buffer (atrans,
      index, out_cbuf[index].fd, out_cbuf[index].buf, atrans->out_info.size, FALSE);
  if (!bret) {
    GST_ERROR_OBJECT (atrans, "queue output buffer failed");
    goto failed;
  }

  return GST_FLOW_OK;

failed:
  /* XXX: omit cleanup */
  return GST_FLOW_ERROR;
}


/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("NV12")
        ";" GST_VIDEO_CAPS_MAKE ("UYVY")
        ";" GST_VIDEO_CAPS_MAKE ("YUYV")
        ";" GST_VIDEO_CAPS_MAKE ("YUY2"))
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("NV12")
        ";" GST_VIDEO_CAPS_MAKE ("UYVY")
        ";" GST_VIDEO_CAPS_MAKE ("YUYV")
        ";" GST_VIDEO_CAPS_MAKE ("YUY2")
        ";" GST_VIDEO_CAPS_MAKE ("ARGB")
        ";" GST_VIDEO_CAPS_MAKE ("xRGB")
        ";" GST_VIDEO_CAPS_MAKE ("ABGR")
        ";" GST_VIDEO_CAPS_MAKE ("xBGR")
        ";" GST_VIDEO_CAPS_MAKE ("RGB")
        ";" GST_VIDEO_CAPS_MAKE ("BGR"))
    );


#define gst_acceltransform_parent_class parent_class
G_DEFINE_TYPE (GstAccelTransform, gst_acceltransform, GST_TYPE_BASE_TRANSFORM);


/* copies the given caps */
static GstCaps *
gst_acceltrans_caps_remove_format_info (GstCaps * caps)
{
  GstStructure *st;
  GstCapsFeatures *f;
  gint i, n;
  GstCaps *res;

  res = gst_caps_new_empty ();

  n = gst_caps_get_size (caps);
  for (i = 0; i < n; i++) {
    st = gst_caps_get_structure (caps, i);
    f = gst_caps_get_features (caps, i);

    /* If this is already expressed by the existing caps
     * skip this structure */
    if (i > 0 && gst_caps_is_subset_structure_full (res, st, f))
      continue;

    st = gst_structure_copy (st);
    /* Only remove format info for the cases when we can actually convert */
    if (!gst_caps_features_is_any (f)
        && gst_caps_features_is_equal (f,
            GST_CAPS_FEATURES_MEMORY_SYSTEM_MEMORY))
      gst_structure_remove_fields (st, "format", "colorimetry", "chroma-site",
          NULL);

    gst_caps_append_structure_full (res, st, gst_caps_features_copy (f));
  }

  return res;
}


/* Answer the allocation query downstream. */
static gboolean
gst_acceltrans_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query)
{
  GstAccelTransform *atrans = GST_ACCEL_TRANSFORM_CAST (trans);
  GstBufferPool *pool;
  GstStructure *config;
  GstCaps *caps;
  guint size;
  gboolean need_pool;

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (caps == NULL)
    goto invalid_caps;

  if (need_pool) {
    GstVideoInfo info;

    if (!gst_video_info_from_caps (&info, caps))
      goto invalid_caps;

    pool = gst_cmem_buffer_pool_new ();

    /* the normal size of a frame */
    size = info.size;
    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_set_params (config, caps, size, CMEM_POOL_MIN_BUF_NUM, CMEM_POOL_MAX_BUF_NUM);
    if (!gst_buffer_pool_set_config (pool, config))
      goto config_failed;

    gst_query_add_allocation_pool (query, pool, size, CMEM_POOL_MIN_BUF_NUM, CMEM_POOL_MAX_BUF_NUM);
    gst_object_unref (pool);
  }

  return TRUE;

  /* ERRORS */
invalid_caps:
  {
    GST_ERROR_OBJECT (atrans, "invalid caps");
    atrans->negotiated = FALSE;
    return FALSE;
  }

config_failed:
  {
    GST_WARNING_OBJECT (atrans, "failed setting config");
    gst_object_unref (pool);
    atrans->negotiated = FALSE;
    return FALSE;
  }
}


/* our output size only depends on the caps, not on the input caps */
static gboolean
gst_acceltrans_transform_size (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, gsize size,
    GstCaps * othercaps, gsize * othersize)
{
  gboolean ret = TRUE;
  GstVideoInfo info;

  g_assert (size);

  ret = gst_video_info_from_caps (&info, othercaps);
  if (ret)
    *othersize = info.size;

  return ret;
}


static GstCaps *
gst_acceltrans_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *tmp, *tmp2;
  GstCaps *result;

  /* Get all possible caps that we can transform to */
  tmp = gst_acceltrans_caps_remove_format_info (caps);

  if (filter) {
    tmp2 = gst_caps_intersect_full (filter, tmp, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (tmp);
    tmp = tmp2;
  }

  result = tmp;

  GST_DEBUG_OBJECT (trans, "transformed %" GST_PTR_FORMAT " into %"
      GST_PTR_FORMAT, caps, result);

  return result;
}


static gboolean
gst_acceltrans_get_unit_size (GstBaseTransform * trans, GstCaps * caps,
    gsize * size)
{
  GstAccelTransform *atrans = GST_ACCEL_TRANSFORM_CAST (trans);
  GstVideoInfo info;

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_WARNING_OBJECT (atrans, "Failed to parse caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }

  *size = info.size;

  GST_DEBUG_OBJECT (atrans, "Returning size %" G_GSIZE_FORMAT " bytes"
      "for caps %" GST_PTR_FORMAT, *size, caps);

  return TRUE;
}


static void
gst_acceltrans_set_property (GObject *object, guint prop_id, GValue const *value, GParamSpec *pspec)
{
  GstAccelTransform *atrans = GST_ACCEL_TRANSFORM_CAST (object);
  const gchar *dev_name;

  switch (prop_id) {
    case PROP_DEVNAME:
      dev_name = g_value_get_string (value);
      g_free (atrans->device_name);
      atrans->device_name = g_strdup (dev_name);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static void
gst_acceltrans_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  GstAccelTransform *atrans = GST_ACCEL_TRANSFORM_CAST (object);

  switch (prop_id) {
    case PROP_DEVNAME:
      g_value_set_string (value, atrans->device_name);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static gboolean
gst_acceltrans_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstAccelTransform *atrans = GST_ACCEL_TRANSFORM_CAST (trans);
  GstVideoInfo in_info, out_info;

  /* input caps */
  if (!gst_video_info_from_caps (&in_info, incaps))
    goto invalid_caps;

  /* output caps */
  if (!gst_video_info_from_caps (&out_info, outcaps))
    goto invalid_caps;

  atrans->in_info = in_info;
  atrans->out_info = out_info;

  if (!setup_device (atrans))
    goto hw_error;
#if 0
  /* XXX: test */
  GST_ERROR_OBJECT (atrans, "input - width:%d/height:%d/format:%#x/fourcc:%#x",
    GST_VIDEO_INFO_WIDTH (&in_info), GST_VIDEO_INFO_HEIGHT (&in_info),
    GST_VIDEO_INFO_FORMAT (&in_info), gst_video_format_to_fourcc (GST_VIDEO_INFO_FORMAT (&in_info)));

  GST_ERROR_OBJECT (atrans, "output - width:%d/height:%d/format:%#x/forcc:%#x",
    GST_VIDEO_INFO_WIDTH (&out_info), GST_VIDEO_INFO_HEIGHT (&out_info),
    GST_VIDEO_INFO_FORMAT (&out_info), gst_video_format_to_fourcc (GST_VIDEO_INFO_FORMAT (&out_info)));
#endif
  atrans->negotiated = TRUE;

  return TRUE;

  /* ERRORS */
invalid_caps:
  {
    GST_ERROR_OBJECT (atrans, "invalid caps");
    atrans->negotiated = FALSE;
    return FALSE;
  }
hw_error:
  {
    GST_WARNING_OBJECT (atrans, "Could not initialize hw.");
    atrans->negotiated = FALSE;
    return FALSE;
  }
}


static GstFlowReturn
gst_acceltrans_transform (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstFlowReturn res;
  GstAccelTransform *atrans = GST_ACCEL_TRANSFORM_CAST (trans);
  GstMemory *mem;
  GstVideoFrame frame;
  guint8 *output;

  if (G_UNLIKELY (!atrans->negotiated))
    goto unknown_format;

  if (!gst_video_frame_map (&frame, &atrans->out_info, outbuf, GST_MAP_WRITE))
    goto map_failed;

  output = GST_VIDEO_FRAME_PLANE_DATA (&frame, 0);

  mem = gst_buffer_peek_memory (inbuf, 0);
  if (GST_IS_CMEM_MEMORY_ALLOCATOR (mem->allocator)) {
    GstCMemMemory *cmem = (GstCMemMemory *)mem;

    res = hwtransform(atrans, cmem, output, TRUE);
  }
  else {
    GstMapInfo map;

    if (!gst_buffer_map (inbuf, &map, GST_MAP_READ)) {
      gst_video_frame_unmap (&frame);
      goto map_failed;
    }

    res = hwtransform(atrans, map.data, output, FALSE);
    gst_buffer_unmap (inbuf, &map);
  }

  gst_video_frame_unmap (&frame);

  return res;

  /* ERRORS */
unknown_format:
  {
    GST_ELEMENT_ERROR (atrans, CORE, NOT_IMPLEMENTED, (NULL),
        ("unknown format"));
    return GST_FLOW_NOT_NEGOTIATED;
  }
map_failed:
  {
    GST_WARNING_OBJECT (atrans, "Could not map buffer, skipping");
    return GST_FLOW_OK;
  }
}


static gboolean
gst_acceltrans_stop (GstBaseTransform *trans)
{
  GstAccelTransform *atrans = GST_ACCEL_TRANSFORM_CAST (trans);

  if (atrans->negotiated) {
    cleanup_device (atrans);
    atrans->negotiated = FALSE;
  }

  if (atrans->allocator) {
    gst_object_unref (atrans->allocator);
    atrans->allocator = NULL;
  }

  if (atrans->work_mem) {
    gst_memory_unref (atrans->work_mem);
    atrans->work_mem = NULL;
  }

  return TRUE;
}

static void
gst_acceltransform_class_init (GstAccelTransformClass * klass)
{
  GObjectClass *object_class;
  GstBaseTransformClass *trans_class;
  GstElementClass *gstelement_class;

  object_class = (GObjectClass *) klass;
  trans_class = (GstBaseTransformClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  object_class->set_property = GST_DEBUG_FUNCPTR(gst_acceltrans_set_property);
  object_class->get_property = GST_DEBUG_FUNCPTR(gst_acceltrans_get_property);

  trans_class->set_caps = GST_DEBUG_FUNCPTR (gst_acceltrans_set_caps);
  trans_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_acceltrans_propose_allocation);
  trans_class->transform_size =
      GST_DEBUG_FUNCPTR (gst_acceltrans_transform_size);
  trans_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_acceltrans_transform_caps);
  trans_class->get_unit_size =
      GST_DEBUG_FUNCPTR (gst_acceltrans_get_unit_size);
  trans_class->transform = GST_DEBUG_FUNCPTR (gst_acceltrans_transform);
  trans_class->stop = GST_DEBUG_FUNCPTR (gst_acceltrans_stop);

  g_object_class_install_property (object_class, PROP_DEVNAME,
    g_param_spec_string ("device-name", "V4L2 devie name", "V4L2 device file name(full path)",
        NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  gst_element_class_set_details_simple(gstelement_class,
    "Colorspace converter",
    "Filter/Converter/Video",
    "HW accelerated video transform plugin",
    "AUTHOR_NAME AUTHOR_EMAIL");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));

  GST_DEBUG_CATEGORY_INIT (gst_acceltransform_debug, "acceltransform", 0,
      "acceltransform");
}

static void
gst_acceltransform_init (GstAccelTransform * atrans)
{
  GST_DEBUG_OBJECT (atrans, "gst_accel_transform_init");

  atrans->input_start = FALSE;
  atrans->negotiated = FALSE;
  atrans->device_name = NULL;
  atrans->allocator = NULL;
  atrans->work_mem = NULL;
  atrans->devfd = -1;

  /* enable QoS */
  gst_base_transform_set_qos_enabled (GST_BASE_TRANSFORM (atrans), TRUE);
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "acceltransform", GST_RANK_NONE,
      GST_TYPE_ACCEL_TRANSFORM);
}


#ifndef PACKAGE
#define PACKAGE "hwacceleratedplugin"
#endif

/* gstreamer looks for this structure to register plugins
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    acceltransform,
    "HW accelerated video transform plugin",
    plugin_init,
    VERSION,
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)
