/* drivers/char/s3c_mem.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * S3C MEM driver for /dev/mem
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/errno.h>	/* error codes */
#include <asm/div64.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/io.h>
#include <linux/sched.h>
#include <asm/irq.h>
#include <linux/slab.h>

#include <linux/mman.h>

#include <linux/unistd.h>
#include <linux/version.h>
#include <mach/map.h>
#include <mach/hardware.h>

#include "s3c_mem.h"

static int flag;

static unsigned int physical_address;

#ifdef USE_DMA_ALLOC
static unsigned int virtual_address;
#endif

int s3c_mem_ioctl(struct inode *inode, struct file *file,
		unsigned int cmd, unsigned long arg)
{
#ifdef USE_DMA_ALLOC
	unsigned long virt_addr;
#else
	unsigned long *virt_addr;
#endif

	struct mm_struct *mm = current->mm;
	struct s3c_mem_alloc param;

	switch (cmd) {
	case S3C_MEM_ALLOC:
		mutex_lock(&mem_alloc_lock);
		if (copy_from_user(&param, (struct s3c_mem_alloc *)arg,
					sizeof(struct s3c_mem_alloc))) {
			mutex_unlock(&mem_alloc_lock);
			return -EFAULT;
		}
		flag = MEM_ALLOC;
		param.vir_addr = do_mmap(file, 0, param.size,
				(PROT_READ|PROT_WRITE), MAP_SHARED, 0);
		DEBUG("param.vir_addr = %08x, %d\n",
						param.vir_addr, __LINE__);
		if (param.vir_addr == -EINVAL) {
			printk(KERN_INFO "S3C_MEM_ALLOC FAILED\n");
			flag = 0;
			mutex_unlock(&mem_alloc_lock);
			return -EFAULT;
		}
		param.phy_addr = physical_address;
#ifdef USE_DMA_ALLOC
		param.kvir_addr = virtual_address;
#endif

		DEBUG("KERNEL MALLOC : param.phy_addr = 0x%X \t "
				"size = %d \t param.vir_addr = 0x%X, %d\n",
				param.phy_addr, param.size, param.vir_addr,
				__LINE__);

		if (copy_to_user((struct s3c_mem_alloc *)arg, &param,
					sizeof(struct s3c_mem_alloc))) {
			flag = 0;
			mutex_unlock(&mem_alloc_lock);
			return -EFAULT;
		}
		flag = 0;
		mutex_unlock(&mem_alloc_lock);

		break;

	case S3C_MEM_CACHEABLE_ALLOC:
		mutex_lock(&mem_cacheable_alloc_lock);
		if (copy_from_user(&param, (struct s3c_mem_alloc *)arg,
					sizeof(struct s3c_mem_alloc))) {
			mutex_unlock(&mem_cacheable_alloc_lock);
			return -EFAULT;
		}
		flag = MEM_ALLOC_CACHEABLE;
		param.vir_addr = do_mmap(file, 0, param.size,
				(PROT_READ|PROT_WRITE), MAP_SHARED, 0);
		DEBUG("param.vir_addr = %08x, %d\n",
				param.vir_addr, __LINE__);
		if (param.vir_addr == -EINVAL) {
			printk(KERN_INFO "S3C_MEM_ALLOC FAILED\n");
			flag = 0;
			mutex_unlock(&mem_cacheable_alloc_lock);
			return -EFAULT;
		}
		param.phy_addr = physical_address;
		DEBUG("KERNEL MALLOC : param.phy_addr = 0x%X"
				" \t size = %d \t param.vir_addr = 0x%X, %d\n",
				param.phy_addr, param.size, param.vir_addr,
				__LINE__);

		if (copy_to_user((struct s3c_mem_alloc *)arg, &param,
					sizeof(struct s3c_mem_alloc))) {
			flag = 0;
			mutex_unlock(&mem_cacheable_alloc_lock);
			return -EFAULT;
		}
		flag = 0;
		mutex_unlock(&mem_cacheable_alloc_lock);

		break;

	case S3C_MEM_SHARE_ALLOC:
		mutex_lock(&mem_share_alloc_lock);
		if (copy_from_user(&param, (struct s3c_mem_alloc *)arg,
					sizeof(struct s3c_mem_alloc))) {
			mutex_unlock(&mem_share_alloc_lock);
			return -EFAULT;
		}
		flag = MEM_ALLOC_SHARE;
		physical_address = param.phy_addr;
		DEBUG("param.phy_addr = %08x, %d\n",
				physical_address, __LINE__);
		param.vir_addr = do_mmap(file, 0, param.size,
				(PROT_READ|PROT_WRITE), MAP_SHARED, 0);
		DEBUG("param.vir_addr = %08x, %d\n",
				param.vir_addr, __LINE__);
		if (param.vir_addr == -EINVAL) {
			printk(KERN_INFO "S3C_MEM_SHARE_ALLOC FAILED\n");
			flag = 0;
			mutex_unlock(&mem_share_alloc_lock);
			return -EFAULT;
		}
		DEBUG("MALLOC_SHARE : param.phy_addr = 0x%X \t "
				"size = %d \t param.vir_addr = 0x%X, %d\n",
				param.phy_addr, param.size, param.vir_addr,
				__LINE__);

		if (copy_to_user((struct s3c_mem_alloc *)arg, &param,
					sizeof(struct s3c_mem_alloc))) {
			flag = 0;
			mutex_unlock(&mem_share_alloc_lock);
			return -EFAULT;
		}
		flag = 0;
		mutex_unlock(&mem_share_alloc_lock);

		break;

	case S3C_MEM_CACHEABLE_SHARE_ALLOC:
		mutex_lock(&mem_cacheable_share_alloc_lock);
		if (copy_from_user(&param, (struct s3c_mem_alloc *)arg,
					sizeof(struct s3c_mem_alloc))) {
			mutex_unlock(&mem_cacheable_share_alloc_lock);
			return -EFAULT;
		}
		flag = MEM_ALLOC_CACHEABLE_SHARE;
		physical_address = param.phy_addr;
		DEBUG("param.phy_addr = %08x, %d\n",
				physical_address, __LINE__);
		param.vir_addr = do_mmap(file, 0, param.size,
				(PROT_READ|PROT_WRITE), MAP_SHARED, 0);
		DEBUG("param.vir_addr = %08x, %d\n",
				param.vir_addr, __LINE__);
		if (param.vir_addr == -EINVAL) {
			printk(KERN_INFO "S3C_MEM_SHARE_ALLOC FAILED\n");
			flag = 0;
			mutex_unlock(&mem_cacheable_share_alloc_lock);
			return -EFAULT;
		}
		DEBUG("MALLOC_SHARE : param.phy_addr = 0x%X \t "
				"size = %d \t param.vir_addr = 0x%X, %d\n",
				param.phy_addr, param.size, param.vir_addr,
				__LINE__);

		if (copy_to_user((struct s3c_mem_alloc *)arg, &param,
					sizeof(struct s3c_mem_alloc))) {
			flag = 0;
			mutex_unlock(&mem_cacheable_share_alloc_lock);
			return -EFAULT;
		}
		flag = 0;
		mutex_unlock(&mem_cacheable_share_alloc_lock);

		break;

	case S3C_MEM_FREE:
		mutex_lock(&mem_free_lock);
		if (copy_from_user(&param, (struct s3c_mem_alloc *)arg,
					sizeof(struct s3c_mem_alloc))) {
			mutex_unlock(&mem_free_lock);
			return -EFAULT;
		}

		DEBUG("KERNEL FREE : param.phy_addr = 0x%X \t "
				"size = %d \t param.vir_addr = 0x%X, %d\n",
				param.phy_addr, param.size, param.vir_addr,
				__LINE__);

		if (do_munmap(mm, param.vir_addr, param.size) < 0) {
			printk(KERN_INFO "do_munmap() failed !!\n");
			mutex_unlock(&mem_free_lock);
			return -EINVAL;
		}

#ifdef USE_DMA_ALLOC
		virt_addr = param.kvir_addr;
		dma_free_writecombine(NULL, param.size,
				(unsigned int *) virt_addr, param.phy_addr);
#else
		virt_addr = (unsigned long *)phys_to_virt(param.phy_addr);
		kfree(virt_addr);
#endif
		param.size = 0;
		DEBUG("do_munmap() succeed !!\n");

		if (copy_to_user((struct s3c_mem_alloc *)arg, &param,
					sizeof(struct s3c_mem_alloc))) {
			mutex_unlock(&mem_free_lock);
			return -EFAULT;
		}

		mutex_unlock(&mem_free_lock);

		break;

	case S3C_MEM_SHARE_FREE:
		mutex_lock(&mem_share_free_lock);
		if (copy_from_user(&param, (struct s3c_mem_alloc *)arg,
					sizeof(struct s3c_mem_alloc))) {
			mutex_unlock(&mem_share_free_lock);
			return -EFAULT; }

		DEBUG("MEM_SHARE_FREE : param.phy_addr = 0x%X \t "
				"size = %d \t param.vir_addr = 0x%X, %d\n",
				param.phy_addr, param.size, param.vir_addr,
				__LINE__);

		if (do_munmap(mm, param.vir_addr, param.size) < 0) {
			printk(KERN_INFO "do_munmap() failed - MEM_SHARE_FREE!!\n");
			mutex_unlock(&mem_share_free_lock);
			return -EINVAL;
		}

		param.vir_addr = 0;
		DEBUG("do_munmap() succeed !! - MEM_SHARE_FREE\n");

		if (copy_to_user((struct s3c_mem_alloc *)arg, &param,
					sizeof(struct s3c_mem_alloc))) {
			mutex_unlock(&mem_share_free_lock);
			return -EFAULT;
		}

		mutex_unlock(&mem_share_free_lock);

		break;

	default:
		DEBUG("s3c_mem_ioctl() : default !!\n");
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(s3c_mem_ioctl);

int s3c_mem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long pageFrameNo = 0, size, phys_addr;

#ifdef USE_DMA_ALLOC
	unsigned long virt_addr;
#else
	unsigned long *virt_addr;
#endif

	size = vma->vm_end - vma->vm_start;

	switch (flag) {
	case MEM_ALLOC:
	case MEM_ALLOC_CACHEABLE:

#ifdef USE_DMA_ALLOC
		virt_addr = (unsigned long)dma_alloc_writecombine(NULL, size,
				(unsigned int *) &phys_addr,
				GFP_KERNEL);
#else
		virt_addr = kmalloc(size, GFP_DMA|GFP_ATOMIC);
#endif
		if (!virt_addr) {
			printk(KERN_INFO "kmalloc() failed !\n");
			return -EINVAL;
		}
		DEBUG("MMAP_KMALLOC : virt addr = 0x%08x, size = %d, %d\n",
						virt_addr, size, __LINE__);

#ifndef USE_DMA_ALLOC
		phys_addr = virt_to_phys((unsigned long *)virt_addr);
#endif
		physical_address = (unsigned int)phys_addr;

#ifdef USE_DMA_ALLOC
		virtual_address = virt_addr;
#endif
		pageFrameNo = __phys_to_pfn(phys_addr);
		break;

	case MEM_ALLOC_SHARE:
	case MEM_ALLOC_CACHEABLE_SHARE:
		DEBUG("MMAP_KMALLOC_SHARE : phys addr = 0x%08x, %d\n",
						physical_address, __LINE__);

/* page frame number of the address for the physical_address to be shared. */
		pageFrameNo = __phys_to_pfn(physical_address);
		DEBUG("MMAP_KMALLOC_SHARE : vma->end = 0x%08x, "
				"vma->start = 0x%08x, size = %d, %d\n",
				vma->vm_end, vma->vm_start, size, __LINE__);
		break;

	default:
		break;
	}

	if ((flag == MEM_ALLOC) || (flag == MEM_ALLOC_SHARE))
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	vma->vm_flags |= VM_RESERVED;

	if (remap_pfn_range(vma, vma->vm_start, pageFrameNo,
						size, vma->vm_page_prot)) {
		printk(KERN_INFO "s3c_mem_mmap() : remap_pfn_range() failed !\n");
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(s3c_mem_mmap);
