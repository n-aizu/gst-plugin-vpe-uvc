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

#include "cmem_buf.h"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <ti/cmem.h>
#include <sys/ioctl.h>

#define CMEM_BLOCKID CMEM_CMABLOCKID

CMEM_AllocParams cmem_alloc_params = {
	CMEM_HEAP,	/* type */
	CMEM_CACHED,	/* flags */
	1		/* alignment */
};

void init_cmem()
{
	CMEM_init();
}

int alloc_cmem_buffer(unsigned int size, unsigned int align, void **cmem_buf)
{
	int fd;
	cmem_alloc_params.alignment = align;

	*cmem_buf = CMEM_alloc2(CMEM_BLOCKID, size,
		&cmem_alloc_params);

	if(*cmem_buf == NULL){
		return -ENOMEM;
	}

	fd = CMEM_export_dmabuf(*cmem_buf);
	return (fd > 0 ? fd : -EFAULT); /* XXX: CMEM_export_dmabuf returns 0 if failed */
}

void free_cmem_buffer(void *cmem_buffer)
{
	CMEM_free(cmem_buffer, &cmem_alloc_params);
}

int cmem_do_cache_operation(void *ptr, size_t size, int cache_operation) 
{
	int ret;

	if (cache_operation == CMEM_CACHE_FLUSH)
		ret = CMEM_cacheWb(ptr, size);
	else
		ret = CMEM_cacheInv(ptr, size);

	return (ret < 0 ? -EFAULT : 0);
}
