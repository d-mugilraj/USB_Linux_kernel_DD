#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/usb/input.h>
#include <linux/hid.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/types.h>
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/fcntl.h>
#include <linux/sched/signal.h>
#include <linux/irq.h>
#include "wait.h"

// #include "linux/kernel/sched/core.c"



#define OURMOUSE_BASE 0x300
unsigned int OURMOUSE_IRQ=50;
#define SPIN(s) spinlock_t s = __SPIN_LOCK_UNLOCKED(s)


//global variables
static int mouse_users = 0; /* User count */
static int mouse_dx = 0; /* Position changes */
static int mouse_dy = 0;
static int mouse_event = 0; /* Mouse has moved */
static int mouse_buttons = 0; /* Mouse has moved */
SPIN(mouse_lock);

static DECLARE_WAIT_QUEUE_HEAD(mouse_wait);



irqreturn_t ourmouse_interrupt(int OURMOUSE_IRQ, void *dev_id, struct pt_regs *regs)
{
	char delta_x;
	char delta_y;
	unsigned char new_buttons;
	delta_x = inb(OURMOUSE_BASE);
	delta_y = inb(OURMOUSE_BASE+1);
	new_buttons = inb(OURMOUSE_BASE+2);
	if(delta_x || delta_y || new_buttons != mouse_buttons)
	{
		/* Something happened */
		spin_lock(&mouse_lock);
		mouse_event = 1;
		mouse_dx += delta_x;
		mouse_dy += delta_y;
		printk(KERN_ALERT "ourmouse: POS x=%d,y=%d.\n",mouse_dx,mouse_dy);
		mouse_buttons = new_buttons;
		spin_unlock(&mouse_lock);
		wake_up_interruptible(&mouse_wait);
	}
	return 0;
}




static ssize_t read_mouse(struct file *file, char *buffer, size_t count, loff_t *pos)
{
	int dx, dy;
	unsigned char button;
	unsigned long flags;
	int n;
	if(count<3)
		return -EINVAL;

	while(!mouse_event)
	{
		if(file -> f_flags & O_NDELAY)
			return -EAGAIN;
		interruptible_sleep_on(&mouse_wait);
		if(signal_pending(current))
			return -ERESTARTSYS;
	}
	/* Grab the event */
	spin_lock_irqsave(&mouse_lock, flags);
	dx = mouse_dx;
	dy = mouse_dy;
	button = mouse_buttons;
	if(dx<=-127)
		dx=-127;
	if(dx>=127)
		dx=127;
	if(dy<=-127)
		dy=-127;
	if(dy>=127)
		dy=127;
	mouse_dx -= dx;
	mouse_dy -= dy;
	if(mouse_dx == 0 && mouse_dy == 0)
		mouse_event = 0;
	spin_unlock_irqrestore(&mouse_lock, flags);

	if(put_user(button|0x80, buffer))
		return -EFAULT;
	if(put_user((char)dx, buffer+1))
		return -EFAULT;
	if(put_user((char)dy, buffer+2))
		return -EFAULT;
	for(n=3; n < count; n++)
		if(put_user(0x00, buffer+n))
			return -EFAULT;
		return count;
}


static int open_mouse(struct inode *inode, struct file *file)
{
	if(mouse_users++)
		return 0;
	if(request_irq(OURMOUSE_IRQ,  irq_handler_t ourmouse_interrupt, 0, "ourmouse", NULL))
	{
		mouse_users--;
		return -EBUSY;
	}
	mouse_dx = 0;
	mouse_dy = 0;
	mouse_event = 0;
	mouse_buttons = 0;
	return 0;
}

static int close_mouse(struct inode *inode, struct file *file)
{
	if(--mouse_users)
		return 0;
	free_irq(OURMOUSE_IRQ, NULL);
	return 0;
}

static ssize_t write_mouse(struct file *file, const char *buffer, size_t count, loff_t *ppos)
{
	return -EINVAL;
}


static unsigned int poll_mouse(struct file *file, poll_table *wait)
{
	poll_wait(file, &mouse_wait, wait);
	if(mouse_event)
		return POLLIN | POLLRDNORM;
	return 0;
}

struct file_operations our_mouse_fops = {
										owner: THIS_MODULE, /* Automatic usage management */
										read: read_mouse, /* You can read a mouse */
										write: write_mouse, /* This won’t do a lot */
										poll: poll_mouse, /* Poll */
										open: open_mouse, /* Called on open */
										release: close_mouse /* Called on close */
										};



static struct miscdevice our_mouse = {OURMOUSE_BASE, "ourmouse", &our_mouse_fops};

static int ourmouse_init(void)
{
	if (request_region(OURMOUSE_BASE, 3, "ourmouse") < 0) 
	{
		printk(KERN_ERR "ourmouse: request_region failed.\n");
		return -ENODEV;
	}
	if (misc_register(&our_mouse) < 0) 
	{
		printk(KERN_ERR "ourmouse: cannot register misc device.\n");
		release_region(OURMOUSE_BASE, 3);
		return -EBUSY;
	}
	return 0;
}


// #ifdef MODULE
// int init_module(void)
// {
// 	if(ourmouse_init()<0)
// 		return -ENODEV;
// 	return 0;
// }
// void cleanup_module(void)
// {
// 	misc_deregister(&our_mouse);
// 	release_region(OURMOUSE_BASE, 3);
// }
// #endif
static void ourmouse_exit(void)
{
	misc_deregister(&our_mouse);
	release_region(OURMOUSE_BASE, 3);
}

module_init(ourmouse_init);
module_exit(ourmouse_exit);