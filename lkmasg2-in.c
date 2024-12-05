/* File:	lkmasg2-in.c
 * Adapted for Linux 5.15 by: John Aedo
 * Skeleton used by group 11
 * Class:	COP4600-SP23
 */

#include <linux/module.h>	  // Core header for modules.
#include <linux/device.h>	  // Supports driver model.
#include <linux/kernel.h>	  // Kernel header for convenient functions.
#include <linux/fs.h>		  // File-system support.
#include <linux/uaccess.h>	  // User access copy function support.
#include <linux/mutex.h>
#define DEVICE_NAME "lkmasg2-in" // Device name.
#define CLASS_NAME "char-in"	  ///< The device class -- this is a character device driver
#define MAX_SIZE 1024         // Max size for our buffer is 1024 as per problem statement

MODULE_LICENSE("GPL");						 ///< The license type -- this affects available functionality
MODULE_AUTHOR("Alanna Hill, Fletcher Morton, Kevin Shook, Alec Bickler");					 ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("lkmasg2 Writer Kernel Module"); ///< The description -- see modinfo
MODULE_VERSION("0.1");						 ///< A version number to inform users

/**
 * Important variables that store data and keep track of relevant information.
 */
static int major_number;
extern struct queue q;

static struct class *lkmasg2Class = NULL;	///< The device-driver class struct pointer
static struct device *lkmasg2Device = NULL; ///< The device-driver device struct pointer

DEFINE_MUTEX(buffer_mutex);
EXPORT_SYMBOL(buffer_mutex);

/**
 * Prototype functions for file operations.
 */
static int open(struct inode *, struct file *);
static int close(struct inode *, struct file *);
static ssize_t write(struct file *, const char *, size_t, loff_t *);
extern void push(const char *s);
extern void pop(int len, char *s);
extern void init(void);

/**
 * File operations structure and the functions it points to.
 */
static struct file_operations fops =
	{
		.owner = THIS_MODULE,
		.open = open,
		.release = close,
		.write = write,
};

/**
 * Initializes module at installation
 */
int init_module(void)
{
	printk(KERN_INFO "lkmasg2: installing module.\n");

	// Allocate a major number for the device.
	major_number = register_chrdev(0, DEVICE_NAME, &fops);
	if (major_number < 0)
	{
		printk(KERN_ALERT "lkmasg2 could not register number.\n");
		return major_number;
	}
	printk(KERN_INFO "lkmasg2: registered correctly with major number %d\n", major_number);

	// Register the device class
	lkmasg2Class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(lkmasg2Class))
	{ // Check for error and clean up if there is
		unregister_chrdev(major_number, DEVICE_NAME);
		printk(KERN_ALERT "Failed to register device class\n");
		return PTR_ERR(lkmasg2Class); // Correct way to return an error on a pointer
	}
	printk(KERN_INFO "lkmasg2: device class registered correctly\n");

	// Register the device driver
	lkmasg2Device = device_create(lkmasg2Class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
	if (IS_ERR(lkmasg2Device))
	{								 // Clean up if there is an error
		class_destroy(lkmasg2Class); // Repeated code but the alternative is goto statements
		unregister_chrdev(major_number, DEVICE_NAME);
		printk(KERN_ALERT "Failed to create the device\n");
		return PTR_ERR(lkmasg2Device);
	}
	printk(KERN_INFO "lkmasg2: device class created correctly\n"); // Made it! device was initialized
	printk(KERN_INFO "lkmasg2 Writer module successfully installed\n");

	// Queue initialization
	init();

	mutex_init(&buffer_mutex);

	return 0;
}

/*
 * Removes module, sends appropriate message to kernel
 */
void cleanup_module(void)
{
	printk(KERN_INFO "lkmasg2: removing module.\n");
	device_destroy(lkmasg2Class, MKDEV(major_number, 0)); // remove the device
	class_unregister(lkmasg2Class);						  // unregister the device class
	class_destroy(lkmasg2Class);						  // remove the device class
	unregister_chrdev(major_number, DEVICE_NAME);		  // unregister the major number
	printk(KERN_INFO "lkmasg2: Goodbye from the LKM!\n");
	unregister_chrdev(major_number, DEVICE_NAME);
	
	mutex_destroy(&buffer_mutex);
	return;
}

/*
 * Opens device module, sends appropriate message to kernel
 */
static int open(struct inode *inodep, struct file *filep)
{
	init_module();

	if(!mutex_trylock(&buffer_mutex)) {
        printk(KERN_ALERT "lkmasg2: device busy with another process");
        return -EBUSY;
    }

	printk(KERN_INFO "lkmasg2: device opened.\n");
	return 0;
}

/*
 * Closes device module, sends appropriate message to kernel
 */
static int close(struct inode *inodep, struct file *filep)
{
	mutex_unlock(&buffer_mutex);

	cleanup_module();
	printk(KERN_INFO "lkmasg2: device closed.\n");
	return 0;
}

int error(char* s) {
	printk(KERN_INFO "lkmasg2 Writer - error encountered %s\n", s);
	return 1;
}

/*
 * Writes to the device
 */
static ssize_t write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
	int tot_len = len;
	int rem_len = len;
	printk(KERN_INFO "lkmasg2 Writer - Entered write().\n");
	

	/* ---------- Protected ---------- */
	// upon aquiring the lock
	mutex_lock(&buffer_mutex);
	printk(KERN_INFO "lkmasg2 Writer - Acquired the lock.\n");

	// upon attempt to write to a full buffer
	printk(KERN_INFO "lkmasg2 Writer - Buffer is full, unable to write.\n");

	// upon writing to buffer, but it is truncated due to insufficient space
	printk(KERN_INFO "lkmasg2 Writer - Buffer has %d bytes remaining, attempting to write %d, truncating input.\n", rem_len, tot_len);

	// upon successful write to the buffer
	push(buffer);
	printk(KERN_INFO "lkmasg2 Writer - Wrote %d bytes to the buffer.\n", tot_len);

	/* ------------------------------- */

	mutex_unlock(&buffer_mutex);
	printk(KERN_INFO "lkmasg2 Writer - Exiting write() function\n");

	// Return the count of the number of bytes attempted to be written
	return tot_len;
}
