#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/smp.h>
#include <linux/mm.h>
#include <asm/io.h>
#include <asm/sbi.h>

#define MAJOR_NO 42
#define CMD_QUEUE_ADDR ((u64)0x90000000)
#define CMD_QUEUE_SIZE (1 << 12)
#define send_saturn_ipi() sbi_ecall(SBI_EXT_IPI, SBI_EXT_IPI_SEND_IPI_RAW, 1, 0, 0, 0, 0, 0)

struct device_info {
	struct cdev cdev;
};

struct device_info info;
dev_t dev;
u64 __iomem *cmdqueue;
unsigned long saturn_hart = 1 << 1;

static int saturn_open(struct inode *inode, struct file *fil) {
	printk("opened /dev/saturn\n");
	fil->private_data = NULL;
	return 0;
}

static int saturn_close(struct inode *inode, struct file *fil) {
	printk("close /dev/saturn\n");
	return 0;
}

static long saturn_ioctl(struct file *fil, unsigned int cmd, unsigned long arg) {
	u64 len;
	u64 bufs[10];
	int i;
	void *phys_addr;
	struct sbiret ipi_ret = {0};

	struct vm_area_struct *vma;

	if (cmd != 0) {
		return -ENOTSUPP;
	}

	if (copy_from_user(&len, (u64 *)arg, 8)) {
		goto fail;
	}
	printk("Got length: %llu\n", len);

	if (copy_from_user(&bufs, (u64 *)(arg + 8), len * 16)) {
		goto fail;
	}

	printk(KERN_INFO "fil->private_data: 0x%llx\n", (u64)fil->private_data);

	for (i = 0; i < len; i++) {
		printk(KERN_INFO "searching for bufs[%d] = 0x%llx\n", i*2, bufs[i*2]);

		phys_addr = NULL;
		for (vma = (struct vm_area_struct *)fil->private_data; vma != NULL; vma = vma->vm_next) {
			if (vma->vm_file == fil && vma->vm_start == bufs[i*2]) {
				phys_addr = vma->vm_private_data;
				break;
			}
		}

		if (phys_addr == NULL) {
			printk(KERN_INFO "bad buffer descriptor\n");
			return -EINVAL;
		}
		else {
			bufs[i*2] = (u64)phys_addr;
		}
	}

	for (i = 0; i < len; i++) {
		writeq(bufs[i*2  ], cmdqueue + i * 2   );
		writeq(bufs[i*2+1], cmdqueue + i * 2 + 1);
	}

	ipi_ret = send_saturn_ipi();
	if (ipi_ret.error)
		printk(KERN_INFO "could not send ipi\n");
	else
		printk(KERN_INFO "ipi sent successfully\n");

	return 0;
fail:
	printk(KERN_INFO "could not copy from userspace\n");
	return -EFAULT;
}

static int saturn_mmap(struct file *fil, struct vm_area_struct *vma) {
	size_t len;
	void *buf;
	int ret;
	unsigned long pfn;

	len = vma->vm_end - vma->vm_start;
	buf = kzalloc(len, GFP_KERNEL);
	if (buf == NULL) {
		printk(KERN_INFO "could not allocate mem\n");
		return -ENOMEM;
	}
	printk(KERN_INFO "allocated %lu bytes at 0x%llu\n", len, (u64)buf);
	pfn = virt_to_phys(buf) >> PAGE_SHIFT;

	vma->vm_private_data = (void *)virt_to_phys(buf);
	vma->vm_flags |= VM_IO;

	// fixme: store vm mapping with the device rather than the file struct
	if (fil->private_data == NULL)
		fil->private_data = vma->vm_mm->mmap;

	ret = remap_pfn_range(vma, vma->vm_start, pfn, len, vma->vm_page_prot);
	if (ret) {
		printk(KERN_INFO "could not perform remap\n");
		return -EIO;
	}

	return 0;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = saturn_open,
	.release = saturn_close,
	.unlocked_ioctl = saturn_ioctl,
	.mmap = saturn_mmap
};

static int __init saturn_init(void) {
	int err;
	printk(KERN_INFO "initing Saturn driver\n");

	err = register_chrdev_region(MKDEV(MAJOR_NO, 0), 1, "saturn_driver");
	if (err)
		return err;

	cdev_init(&info.cdev, &fops);
	cdev_add(&info.cdev, MKDEV(MAJOR_NO, 0), 1);

	cmdqueue = ioremap(CMD_QUEUE_ADDR, CMD_QUEUE_SIZE);
	writeq(1, cmdqueue);

	return 0;
}

void saturn_handle_interrupt(void) {
	riscv_clear_ipi();

	printk(KERN_INFO "handling SATURN irq\n");
}

device_initcall(saturn_init);
