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

#ifndef CMEM_BUF_H
#define CMEM_BUF_H

#include <stdint.h>
#include <stdlib.h>

#define CMEM_CACHE_FLUSH      0
#define CMEM_CACHE_INVALIDATE 1

void init_cmem();
int alloc_cmem_buffer(unsigned int size, unsigned int align, void **cmem_buf);
void free_cmem_buffer(void *cmem_buffer);
int cmem_do_cache_operation(void *ptr, size_t size, int cache_operation);

#endif //CMEM_BUF_H
