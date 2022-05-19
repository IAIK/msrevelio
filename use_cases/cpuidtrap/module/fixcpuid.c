#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kallsyms.h>
#include <asm/uaccess.h>
#include <asm/msr.h>
#include "fixcpuid.h"

MODULE_AUTHOR("");
MODULE_DESCRIPTION("Device to trap CPUID");
MODULE_LICENSE("GPL");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#define from_user raw_copy_from_user
#define to_user raw_copy_to_user
#else
#define from_user copy_from_user
#define to_user copy_to_user
#endif

static bool device_busy = false;


static int device_open(struct inode *inode, struct file *file) {
  int j;
    /* Check if device is busy */
  if (device_busy == true) {
    return -EBUSY;
  }

  /* Lock module */
  try_module_get(THIS_MODULE);

  device_busy = true;

  return 0;
}

static int device_release(struct inode *inode, struct file *file) {
  /* Unlock module */
  device_busy = false;

  module_put(THIS_MODULE);

  return 0;
}

static long device_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param) {
 int j;
 size_t start, end, sum = 0;
  switch (ioctl_num) {
    case FIXCPUID_IOCTL_CMD_TRAP_CPUID:
    {
        size_t val;
        rdmsrl(0x140, val);
        val = (val & ~(size_t)1) | (ioctl_param & 1);
        wrmsrl(0x140, val);
        return 0;
    }
    default:
        return -1;
  }

  return 0;
}

static struct file_operations f_ops = {.unlocked_ioctl = device_ioctl,
                                       .open = device_open,
                                       .release = device_release};

static struct miscdevice misc_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = FIXCPUID_DEVICE_NAME,
    .fops = &f_ops,
    .mode = S_IRWXUGO,
};

int init_module(void) {
  int r;

  /* Register device */
  r = misc_register(&misc_dev);
  if (r != 0) {
    printk(KERN_ALERT "[fixcpuid-module] Failed registering device with %d\n", r);
    return 1;
  }

  printk(KERN_INFO "[fixcpuid-module] Loaded.\n");

  return 0;
}

void cleanup_module(void) {
  misc_deregister(&misc_dev);

  printk(KERN_INFO "[fixcpuid-module] Removed.\n");
}


