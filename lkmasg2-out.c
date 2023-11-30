/* File:	lkmasg2-out.c
 * Adapted for Linux 5.15 by: John Aedo
 * Skeleton used by group 11
 * Class:	COP4600-SP23
 */

#include <linux/module.h>	  		// Core header for modules.
#include <linux/device.h>	  		// Supports driver model.
#include <linux/kernel.h>	  		// Kernel header for convenient functions.
#include <linux/fs.h>		  		// File-system support.
#include <linux/uaccess.h>	  		// User access copy function support.
#include <linux/mutex.h>            // Mutex library for synchronization
#include <linux/version.h>          // Version macros
#include <string.h>
#define DEVICE_NAME "lkmasg2-out"   // Device name.
#define CLASS_NAME "char-out"	    ///< The device class -- this is a character device driver
#define MAX_SIZE 1024         	 	// Max size for our buffer is 1024 as per problem statement

MODULE_LICENSE("GPL");						 ///< The license type -- this affects available functionality
MODULE_AUTHOR("Alanna Hill, Fletcher Morton, Kevin Shook, Alec Bickler");					 ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("lkmasg2 Reader Kernel Module"); ///< The description -- see modinfo
MODULE_VERSION("0.1");						 ///< A version number to inform users

/**
 * Important variables that store data and keep track of relevant information.
 */
static int major_number;
static char sendtoUser[MAX_SIZE] = {0};   // The message we want to send back to the user after consulting the buffer
extern char q_buffer[MAX_SIZE];
extern int q_start;
extern int q_end;
extern int q_size;
extern struct mutex buffer_mutex;

static struct class *lkmasg2Class = NULL;	///< The device-driver class struct pointer
static struct device *lkmasg2Device = NULL; ///< The device-driver device struct pointer

/**
 * Prototype functions for file operations.
 */
static int open(struct inode *, struct file *);
static int close(struct inode *, struct file *);
static ssize_t read(struct file *, char *, size_t, loff_t *);


// This is a queue struct for managing the data
typedef struct {
	char buffer[MAX_SIZE];
	int start;
	int end;
	int size;
} queue;

queue q;

// This function takes in a string, and adds as many characters from the string
// into our buffer as it can without overflowing.
void push(const char *s) {
	int sz = strlen(s);
	int i;
	sz = min(sz, MAX_SIZE - q.size);
	for (i = 0; i < sz; i++) {
		q.buffer[q.end] = s[i];
		q.end = (q.end + 1) % MAX_SIZE;
		q.size++;
	}
}

// This function takes in a desired string length, and a string, and puts a
// string of size max(len, q size) in the given string
void pop(int len, char *s) {
	int i;
	len = max(len, q.size);
	for (i = 0; i < len; i++) {
		s[i] = q.buffer[q.start % MAX_SIZE];
		q.start = (q.start + 1) % MAX_SIZE;
		q.size--;
	}
	s[i] = '\0';
}

/**
 * File operations structure and the functions it points to.
 */
static struct file_operations fops =
	{
		.owner = THIS_MODULE,
		.open = open,
		.release = close,
		.read = read,
};

/**
 * Initializes module at installation
 */
int init_module(void)
{
	printk(KERN_INFO "lkmasg2 installing module.\n");

	// Allocate a major number for the device.
	major_number = register_chrdev(0, DEVICE_NAME, &fops);
	if (major_number < 0)
	{
		printk(KERN_ALERT "lkmasg2 could not register number.\n");
		return major_number;
	}
	printk(KERN_INFO "lkmasg2 registered correctly with major number %d\n", major_number);

	// Register the device class
	lkmasg2Class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(lkmasg2Class))
	{ // Check for error and clean up if there is
		unregister_chrdev(major_number, DEVICE_NAME);
		printk(KERN_ALERT "Failed to register device class\n");
		return PTR_ERR(lkmasg2Class); // Correct way to return an error on a pointer
	}
	printk(KERN_INFO "lkmasg2 device class registered correctly\n");

	// Register the device driver
	lkmasg2Device = device_create(lkmasg2Class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
	if (IS_ERR(lkmasg2Device))
	{								 // Clean up if there is an error
		class_destroy(lkmasg2Class); // Repeated code but the alternative is goto statements
		unregister_chrdev(major_number, DEVICE_NAME);
		printk(KERN_ALERT "Failed to create the device\n");
		return PTR_ERR(lkmasg2Device);
	}
	printk(KERN_INFO "lkmasg2 Reader module successfuly installed\n"); // Made it! device was initialized

	return 0;
}

/*
 * Removes module, sends appropriate message to kernel
 */
void cleanup_module(void)
{
	// printk(KERN_INFO "lkmasg2: removing module.\n");
	device_destroy(lkmasg2Class, MKDEV(major_number, 0)); // remove the device
	class_unregister(lkmasg2Class);						  // unregister the device class
	class_destroy(lkmasg2Class);						  // remove the device class
	
	unregister_chrdev(major_number, DEVICE_NAME);		  // unregister the major number
	// printk(KERN_INFO "lkmasg2: Goodbye from the LKM!\n");
	unregister_chrdev(major_number, DEVICE_NAME);
	return;
}

/*
 * Opens device module, sends appropriate message to kernel
 */
static int open(struct inode *inodep, struct file *filep)
{
	init_module();

	// Queue initialization
	q.buffer[0] = '\0';
	q.size = 0;
	q.start = 0;
	q.end = 0;
	
	if(!mutex_trylock(&buffer_mutex)) {
		printk(KERN_ALERT "lkmasg2: device busy with another process.\n");
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
	printk(KERN_INFO "lkmasg2 Reader - error encountered %s\n", s);
	return 1;
}

/*
 * Reads from device, displays in userspace, and deletes the read data
 */
static ssize_t read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
	int tot_len, act_len;
	int error = 0;

	// Update Queue
	strcpy(q.buffer, q_buffer);
	q.size = q_size;
	q.start = q_start;
	q.end = q_end;

	printk(KERN_INFO "lkmasg2 Reader - Entered read()\n");

	/* ---------- Protected ---------- */
	// upon successfuly acquiring the lock
	mutex_lock(&buffer_mutex);
	printk(KERN_INFO "lkmasg2 Reader - Acquired the lock.\n");

	tot_len, act_len = len;

	// upon attempting to read an empty buffer
	printk(KERN_INFO "lkmasg2 Reader - Buffer is empty, unable to read.\n");

	// upon truncated read due to requesting more bytes than available
	printk(KERN_INFO "lkmasg2 Reader - Buffer has %d bytes of content, requested %d\n", act_len, tot_len);

	//Pop a string from the buffer and store it in sendtoUser
	pop(act_len, sendtoUser);

	// 0 for success, any other value indicates an error occurred
	error = copy_to_user(buffer, sendtoUser, strlen(sendtoUser));

	if (error == 0) {
		// upon successful read form buffer
		printk(KERN_INFO "lkmasg2 Reader - Read %d bytes from the buffer\n", tot_len);

		mutex_unlock(&buffer_mutex);
		printk(KERN_INFO "lkmasg2 Reader - Exiting read() function\n");
		return 0;
	
	} else {
		// Some fail msg??
		printk(KERN_INFO "lkmasg2 Reader - Failed to send the string to the user\n");

		mutex_unlock(&buffer_mutex);
		printk(KERN_INFO "lkmasg2 Reader - Exiting read() function\n");
      	return -EFAULT; // Returns a bad address to indicate the error
	}

	/* ------------------------------- */

}
