#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>

#define MAJOR_NO 42

struct device_info {
	struct cdev cdev;
};

struct device_info info;
dev_t dev;

static int saturn_open(struct inode *inode, struct file *file) {
	printk("opened /dev/saturn\n");
	return 0;
}

static long saturn_write(struct file *file, const char __user *user_buffer, size_t size, loff_t *offset) {
	printk("write /dev/saturn\n");
	return 0;
}


static long saturn_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
	return 0;
}

struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = saturn_open,
	.write = saturn_write,
	.unlocked_ioctl = saturn_ioctl
};

static int __init saturn_init(void)
{
	int err;
	printk("hello test app module init");

	err = register_chrdev_region(MKDEV(MAJOR_NO, 0), 1, "saturn_driver");
	if (err)
		return err;

	cdev_init(&info.cdev, &fops);
	cdev_add(&info.cdev, MKDEV(MAJOR_NO, 0), 1);

	return 0;
}

device_initcall(saturn_init);
