/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

/**
 * References: referred chatGPT to understand how to write read and write driver function in c
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "aesdchar.h"
#include "aesd_ioctl.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Ashwini Patil"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev = NULL;
    PDEBUG("open");

    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;
    
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    filp->private_data = NULL;
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, 
                loff_t *f_pos)
{
    ssize_t retval = 0;
    size_t entry_offset = 0;
    size_t remaining_bytes = 0;
    ssize_t read_bytes = 0;
    struct aesd_buffer_entry *entry = NULL;
    struct aesd_dev *dev = NULL;

    // Log the read request for debugging
    PDEBUG("read %zu bytes with offset %lld", count, *f_pos);

    dev = filp->private_data;
    if (0 != mutex_lock_interruptible(&dev->lock))
    {
        PDEBUG("error in acquiring lock for mutex_lock_interruptible");
        return -ERESTARTSYS;
    }

    // Find the buffer entry and offset for the current file position
    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&aesd_device.buffer, *f_pos, &entry_offset);

    if (entry == NULL) {
        //End of file reached, return 0
        retval = 0;
    } else {
        remaining_bytes = entry->size - entry_offset;
        read_bytes = min(count, remaining_bytes);

        if (copy_to_user(buf, entry->buffptr + entry_offset, read_bytes)) {
            // Error while copying data to user space
            PDEBUG("ERROR: copy_to_user failed!");
            retval = -EFAULT;
        } else {
            // Update the file position and return the number of bytes read
            *f_pos += read_bytes;
            retval = read_bytes;
        }
    }

    mutex_unlock(&dev->lock);
    return retval;
}


ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, 
                loff_t *f_pos)
{
    ssize_t retval = 0;
    const char *replaced_entry_buff = NULL;
    struct aesd_dev *dev;

    // Check for invalid arguments
    if (!filp || !buf) {
        PDEBUG("ERROR: Invalid arguments in aesd_write");
        return -EINVAL;
    }

    PDEBUG("write %zu bytes with offset %lld", count, *f_pos);

    dev = filp->private_data;

    // Acquire a lock for synchronization
    if (mutex_lock_interruptible(&dev->lock)) {
        PDEBUG("ERROR: Failed to acquire lock in aesd_write");
        return -ERESTARTSYS;
    }

    // Attempt to allocate memory for the new data
    char *new_buffptr = krealloc(dev->entry.buffptr, (dev->entry.size + count), GFP_KERNEL);
    if (!new_buffptr) {
        PDEBUG("ERROR: Memory allocation (krealloc) failed in aesd_write");
        mutex_unlock(&dev->lock);
        return -ENOMEM;
    }

    // Copy data from user space to kernel space
    retval = copy_from_user(new_buffptr + dev->entry.size, buf, count);
    if (retval) {
        PDEBUG("ERROR: copy_from_user failed in aesd_write, retval=%zu", retval);
    } else {
        // Data copied successfully
        dev->entry.buffptr = new_buffptr;
        dev->entry.size += count;

        // Check if the data ends with a newline character
        if (buf[count - 1] == '\n') {
            replaced_entry_buff = aesd_circular_buffer_add_entry(&dev->buffer, &dev->entry);

            // Free any overwritten entry's buffer
            if (replaced_entry_buff) {
                kfree(replaced_entry_buff);
            }

            // Reset the working entry
            dev->entry.buffptr = NULL;
            dev->entry.size = 0;
        }
    }

    // Release the lock and return the result
    mutex_unlock(&dev->lock);
    return retval ? retval : count;
}

loff_t aesd_llseek(struct file *filp, loff_t off, int whence)
{
    struct aesd_dev *dev = NULL;
    uint8_t index = 0;
    loff_t file_offset = 0;
    loff_t total_size = 0;
    struct aesd_buffer_entry *entry = NULL;
    
    dev = filp->private_data;
	
    // acquiring lock
    if (0 != mutex_lock_interruptible(&dev->lock))
    {
        PDEBUG("ERROR: mutex_lock_interruptible acquiring lock");
        return -ERESTARTSYS;
    }

    AESD_CIRCULAR_BUFFER_FOREACH(entry,&aesd_device.buffer,index)
    {
        total_size += entry->size;
    }
	
    // releasing lock
    mutex_unlock(&dev->lock);
    file_offset = fixed_size_llseek(filp, off, whence, total_size);
    return file_offset;
}

static long aesd_adjust_file_offset(struct file *filp, unsigned int write_cmd, unsigned int write_cmd_offset)
{
    long retval = 0;
    struct aesd_dev *dev = filp->private_data;
    uint8_t index = 0;

    PDEBUG("aesd_adjust_file_offset()");

    // Acquire lock
    if (mutex_lock_interruptible(&dev->lock) != 0)
    {
        PDEBUG("mutex_lock_interruptible() acquiring lock error");
        return -ERESTARTSYS;
    }

    PDEBUG("aesd_adjust_file_offset() start");

    // Check if write_cmd exceeds max writes supported
    if (write_cmd > AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
    {
        PDEBUG("invalid");
        retval = -EINVAL;
    }

    AESD_CIRCULAR_BUFFER_FOREACH(entry,&aesd_device.buffer,index){}
    
    // Check if write_cmd exceeds the number of entries present
    else if (write_cmd > index)
    {
        PDEBUG("invalid");
        retval = -EINVAL;
    }
    // Check if offset exceeds the size of the entry
    else if (write_cmd_offset >= dev->buffer.entry[write_cmd].size)
    {
        PDEBUG("invalid");
        retval = -EINVAL;
    }
    else
    {
        // Adjust the file offset position
        filp->f_pos = write_cmd_offset;
        int i;
        for (i = 0; i < write_cmd; i++)
        {
            filp->f_pos += dev->buffer.entry[i].size;
        }
        PDEBUG("aesd_adjust_file_offset() completed");
    }

    // Release lock
    mutex_unlock(&dev->lock);
    return retval;
}

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long retval = 0;
 	struct aesd_seekto aesd_data;
 	
	if (_IOC_TYPE(cmd) != AESD_IOC_MAGIC)
	{
		return -ENOTTY;
	}
	if (_IOC_NR(cmd) > AESDCHAR_IOC_MAXNR)
	{
		return -ENOTTY;
	}

 	switch (cmd)
 	{
		case AESDCHAR_IOCSEEKTO:
			retval = copy_from_user(&aesd_data, (const void __user *)arg, sizeof(aesd_data));
        		if (retval != 0)
        		{
            			retval = -EFAULT;
        		}
        		else
        		{
				        retval = aesd_adjust_file_offset(filp, aesd_data.write_cmd, aesd_data.write_cmd_offset);
        		}
        	break;

 	    	default:
 			retval = -ENOTTY;
 			break;
 	}
 	return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek = aesd_llseek,
    .unlocked_ioctl = aesd_ioctl
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    mutex_init(&aesd_device.lock);
    aesd_circular_buffer_init(&aesd_device.buffer);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);
    uint8_t index = 0;
    struct aesd_buffer_entry *entry = NULL;
    cdev_del(&aesd_device.cdev);

    mutex_destroy(&aesd_device.lock);
    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.buffer, index) {
    
        if (entry->buffptr) {
            kfree(entry->buffptr);
            entry->buffptr = NULL;
        }
    }
    unregister_chrdev_region(devno, 1);
}


module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
