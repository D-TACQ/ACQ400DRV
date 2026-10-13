/* ------------------------------------------------------------------------- *
 * maxbufs.c  		                     	                    
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2014 Peter Milne, D-TACQ Solutions Ltd                
 *                      <peter dot milne at D hyphen TACQ dot com>          
 *                         www.d-tacq.com
 *   Created on: 24 May 2014  
 *    Author: pgm                                                         
 *                                                                           *
 *  This program is free software; you can redistribute it and/or modify     *
 *  it under the terms of Version 2 of the GNU General Public License        *
 *  as published by the Free Software Foundation;                            *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *  GNU General Public License for more details.                             *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  along with this program; if not, write to the Free Software              *
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/* ------------------------------------------------------------------------- */

#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/moduleparam.h>

#include <linux/dma-direction.h>
#include "hbm.h"

#define REVID "0.10"

int buffer_len = 0x100000;
module_param(buffer_len, int, 0644);

int number_get_pages_buffers = 0;
module_param(number_get_pages_buffers, int, 0644);

int number_cma_buffers = 0;
module_param(number_cma_buffers, int, 0644);

struct list_head gpb_buffers;
struct list_head cma_buffers;

static void __exit maxbufs_exit(void)
{
	hbm_free(0, &gpb_buffers);
	hbm_free_cma(0, &cma_buffers);
	printk("Good bye\n");
}

static int __init maxbufs_init(void)
{
	INIT_LIST_HEAD(&gpb_buffers);
	INIT_LIST_HEAD(&cma_buffers);

	printk("attempt allocate _get_free_pages %d x %d bytes\n",
	       number_get_pages_buffers, buffer_len);

	if (number_get_pages_buffers > 0) {
		if (hbm_allocate(0, number_get_pages_buffers, buffer_len,
				 &gpb_buffers, DMA_FROM_DEVICE)) {
			printk("ERROR: failed to allocate\n");
			return -1;
		}
	}
	printk("attempt allocate dma_alloc_from_contiguous %d x %d bytes\n",
	       number_cma_buffers, buffer_len);
	if (number_cma_buffers > 0) {
		if (hbm_allocate_cma(0, number_cma_buffers, buffer_len,
				     &cma_buffers, DMA_FROM_DEVICE)) {
			printk("ERROR: failed to allocate\n");
			return -1;
		}
	}
	printk("Hello\n");
	return 0;
}

module_init(maxbufs_init);
module_exit(maxbufs_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("find the limits of kernel memory");
MODULE_AUTHOR("D-TACQ Solutions.");
MODULE_VERSION(REVID);
