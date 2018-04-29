/* Copyright (C) 2014 Texas Instruments Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <linux/videodev2.h>
#include <linux/v4l2-controls.h>

#include <sys/ioctl.h>

#ifdef DEBUG
#define ERROR(fmt, ...) \
	do { fprintf(stderr, "ERROR:%s:%d: " fmt "\n", __func__, __LINE__,\
##__VA_ARGS__); } while (0)
#else
#define ERROR(fmt, ...)
#endif


int v4l2_request_buffer(int devfd,
		int width, int height, int fourcc, int clrspc,
		unsigned int num, int is_input, uint32_t *sizeimage)
{
	struct v4l2_format fmt;
	struct v4l2_requestbuffers reqbuf;
	struct v4l2_buffer vbuffer;
	struct v4l2_plane buf_plane;

	uint32_t type;
	int i, ret;

	if (is_input)
		type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	else
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = type;
	fmt.fmt.pix_mp.width = width;
	fmt.fmt.pix_mp.height = height;
	fmt.fmt.pix_mp.pixelformat = fourcc;
	fmt.fmt.pix_mp.colorspace = clrspc;
	fmt.fmt.pix_mp.num_planes = 1;
	fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;

	ret = ioctl(devfd, VIDIOC_S_FMT, &fmt);
	if (ret < 0) {
		ERROR("VIDIOC_S_FMT failed: %s (%d)", strerror(errno), ret);
		return -1;
	}

	*sizeimage = fmt.fmt.pix_mp.plane_fmt[0].sizeimage;

	memset(&reqbuf, 0, sizeof(reqbuf));
	reqbuf.type = type;
	reqbuf.memory = V4L2_MEMORY_DMABUF;
	reqbuf.count = num;

	ret = ioctl(devfd, VIDIOC_REQBUFS, &reqbuf);
	if (ret < 0) {
		ERROR("VIDIOC_REQBUFS failed: %s (%d)", strerror(errno), ret);
		return -1;
	}

	if (reqbuf.count != num ||
		reqbuf.type != type || reqbuf.memory != V4L2_MEMORY_DMABUF) {
			ERROR("unsupported..");
			return -1;
	}

	memset(&buf_plane, 0, sizeof(buf_plane));
	buf_plane.length = *sizeimage;

	if (is_input)
		buf_plane.bytesused = *sizeimage;
	else
		buf_plane.bytesused = 0;

	memset(&vbuffer, 0, sizeof(vbuffer));
	vbuffer.type = type;
	vbuffer.memory = V4L2_MEMORY_DMABUF;
	vbuffer.field = V4L2_FIELD_ANY;
	vbuffer.m.planes = &buf_plane;
	vbuffer.length = 1;

	for (i = 0; i < num; i++) {
		vbuffer.index = i;

		ret = ioctl(devfd, VIDIOC_QUERYBUF, &vbuffer);
		if (ret < 0) {
			ERROR("VIDIOC_QUERYBUF failed: %s (%d)", strerror(errno), ret);
			return -1;
		}
	}

	return 0;
}


int v4l2_queue_buffer(int devfd, int buf_idx, int dma_fd, uint32_t sizeimage, int is_input)
{
	int ret;
	struct v4l2_buffer buffer;
	struct v4l2_plane buf_plane;
	uint32_t type, bytesused;

	if (is_input) {
		type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		bytesused = sizeimage;
	}
	else {
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		bytesused = 0;
	}

	memset(&buf_plane, 0, sizeof(buf_plane));
	buf_plane.m.fd = dma_fd;
	buf_plane.bytesused = bytesused;
	buf_plane.length = sizeimage;

	memset(&buffer, 0, sizeof(buffer));
	buffer.type = type;
	buffer.memory = V4L2_MEMORY_DMABUF;
	buffer.index = buf_idx;
	buffer.field = V4L2_FIELD_ANY;
	buffer.m.planes = &buf_plane;
	buffer.length = 1;

	ret = ioctl(devfd, VIDIOC_QBUF, &buffer);
	if (ret < 0) {
		ERROR("VIDIOC_QBUF failed: %s (%d)", strerror(errno), ret);
		return -1;
	}

	return 0;
}


int v4l2_dequeue_buffer(int devfd, int is_input)
{
	struct v4l2_buffer buffer;
	struct v4l2_plane buf_plane;
	uint32_t type;
	int ret;

	if (is_input)
		type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	else
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	memset(&buf_plane, 0, sizeof(buf_plane));

	memset(&buffer, 0, sizeof(buffer));
	buffer.type	= type;
	buffer.memory = V4L2_MEMORY_DMABUF;
	buffer.m.planes = &buf_plane;
	buffer.length = 1;

	ret = ioctl(devfd, VIDIOC_DQBUF, &buffer);
	if (ret < 0) {
		ERROR("VIDIOC_DQBUF failed: %s (%d)", strerror(errno), ret);
		return -1;
	}

	return (int)buffer.index;
}


int v4l2_stream_on(int devfd, int is_input)
{
	int ret;
	uint32_t type;

	if (is_input)
		type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	else
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	ret = ioctl(devfd, VIDIOC_STREAMON, &type);
	if (ret)
		ERROR("VIDIOC_STREAMON failed: %s (%d)", strerror(errno), ret);

	return ret;
}


int v4l2_stream_off(int devfd, int is_input)
{
	int ret;
	uint32_t type;

	if (is_input)
		type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	else
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	ret = ioctl(devfd, VIDIOC_STREAMOFF, &type);
	if (ret)
		ERROR("VIDIOC_STREAMOFF failed: %s (%d)", strerror(errno), ret);

	return ret;
}

