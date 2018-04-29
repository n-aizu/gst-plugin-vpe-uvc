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

#ifndef V4L2_M2M_H
#define V4L2_M2M_H

int v4l2_request_buffer(int devfd,
		int width, int height, int fourcc, int clrspc,
		unsigned int num, int is_input, uint32_t *sizeimage);
int v4l2_queue_buffer(int devfd, int buf_idx, int dma_fd, uint32_t sizeimage, int is_input);
int v4l2_dequeue_buffer(int devfd, int is_input);
int v4l2_stream_on(int devfd, int is_input);
int v4l2_stream_off(int devfd, int is_input);

#endif /* V4L2_M2M_H */
