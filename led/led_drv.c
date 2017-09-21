#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/wait.h>
#include <asm/io.h>
#include <asm/atomic.h>
#include <linux/slab.h>
#include <linux/sched.h>


#include "led_cmd.h"

#define GPFCON	0x56000050
#define GPFDAT	0x56000054
#define GPFPUD	0x56000058

struct gpio_resources {
	unsigned long  con;
	unsigned long  dat;
	unsigned long  pud;
};

struct led{
	dev_t devno;
	int minor;
	int count;
	struct cdev *cdev;
	struct class *cls;
	struct device *device;
	struct gpio_resources *gpf;
	wait_queue_head_t wait_head;
	atomic_t led_available;
};

static struct led g_led={
	.led_available = ATOMIC_INIT(1),
};

static int led_on(int i)
{
	switch(i)
	{
		case 1:
			writel(readl(&g_led.gpf->dat)&(~(0x1<<4)),&g_led.gpf->dat);
			break;
		case 2:
			writel(readl(&g_led.gpf->dat)&(~(0x1<<5)),&g_led.gpf->dat);
			break;
		case 3:
			writel(readl(&g_led.gpf->dat)&(~(0x1<<6)),&g_led.gpf->dat);
			break;
		default:
			break;
	}

	return 0;
}

static int led_off(int i)
{
	switch(i)
	{
		case 1:
			writel(readl(&g_led.gpf->dat)|(0x1<<4),&g_led.gpf->dat);
			break;
		case 2:
			writel(readl(&g_led.gpf->dat)|(0x1<<5),&g_led.gpf->dat);
			break;
		case 3:
			writel(readl(&g_led.gpf->dat)|(0x1<<6),&g_led.gpf->dat);
			break;
		default:
			break;
	}

	return 0;
}

static int led_hw_init(struct led *led,int minor,int count,const struct file_operations *fops)
{
	int ret;
	led->minor = minor;
	led->count = count;
	ret = alloc_chrdev_region(&led->devno, led->minor, led->count,"led");
	if(ret!=0){
		printk(KERN_ERR "alloc_chrdev_region error.\n");
		return ret;
	}
	led->cdev = cdev_alloc();
	if(!led->cdev){
		printk("cdev_alloc error.\n");
		goto err1;
	}

	cdev_init(led->cdev,fops);
	led->cdev->owner = THIS_MODULE;

	ret = cdev_add(led->cdev,led->devno,led->count);
	if(ret){
		printk("cdev_add error.\n");
		goto err2;
	}

	led->cls = class_create(THIS_MODULE, "led");
	if(IS_ERR(led->cls)){
		printk("class_create error.\n");
		ret = PTR_ERR(led->cls);
		goto err3;
	}
	led->device = device_create(led->cls,NULL,led->devno, NULL,"led");
	if(IS_ERR(led->device)){
		printk("device_create error,\n");
		ret = PTR_ERR(led->device);
		goto err4;
	}
	
	led->gpf = ioremap(GPFCON, sizeof(struct gpio_resources));
	if(!led->gpf){
		printk("ioremap error.\n");
		ret = -ERESTARTSYS;
		goto err5;
	}
	// set GPFDAT 4 5 6 '1'
	writel(readl(&led->gpf->dat)|(0x7<<4),&led->gpf->dat);

	// set GPFCON
	writel(readl(&led->gpf->con)|(0x15<<4),&led->gpf->dat);
	
	//atomic_set(&led->led_available,1);
	//led->led_available = ATOMIC_INIT(1); // 设置原子变量
	init_waitqueue_head(&led->wait_head);
	return 0;
err5:
	device_destroy(led->cls, led->devno);
err4:
	class_destroy(led->cls);
err3:
	cdev_del(led->cdev);
err2:
	kfree(led->cdev);
err1:
	unregister_chrdev_region(led->devno, led->count);
	return ret;

}

static void led_destroy(void)
{
	writel(readl(&g_led.gpf->dat)|(0x7<<4),&g_led.gpf->dat);
	iounmap(g_led.gpf);
}


static int led_is_available(void)
{
	if(!atomic_dec_and_test(&g_led.led_available))
	{
		atomic_inc(&g_led.led_available);-7 n
		return 0;                                
	}

	return 1;
}	

static int led_open(struct inode *inode, struct file *filep)
{

	if(!atomic_dec_and_test(&g_led.led_available))
	{
		atomic_inc(&g_led.led_available);
		if(filep->f_flags & O_NONBLOCK){
			printk("led is unavailable.\n");
			return -EBUSY;
		}
		wait_event_interruptible(g_led.wait_head, 1==led_is_available());
	}
	printk("led_open .\n");
	return 0;
}


static long led_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	switch(cmd)
	{
		case IOCTL_LED_ON:
			led_on(arg);
			break;
		case IOCTL_LED_OFF:
			led_off(arg);
			break;
		default:
			break;
	}
	return 0;
}

static int led_release(struct inode *inode, struct file *filep)
{
	atomic_inc(&g_led.led_available);
	wake_up(&g_led.wait_head); //唤醒等待队列
	return 0;
}



static const struct file_operations fops={
	.owner = THIS_MODULE,
	.open = led_open,
	.release = led_release,
	.unlocked_ioctl = led_ioctl,
};


static int __init led_init(void)
{
	int ret;	
	ret = led_hw_init(&g_led, 0, 1,&fops);
	if(ret!=0){
		printk("led_hw_init error.\n");
		return ret;
	}
	return 0;
}

static void __exit led_exit(void)
{
	
	device_destroy(g_led.cls, g_led.devno);
	class_destroy(g_led.cls);
	cdev_del(g_led.cdev);
	kfree(g_led.cdev);
	unregister_chrdev_region(g_led.devno, g_led.count);
	led_destroy();
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");



