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
#include <linux/interrupt.h>
#include <asm/string.h>
#include <linux/uaccess.h>

struct key_t{
	dev_t devno;
	int minor;
	int count;
	struct cdev *cdev;
	struct class *cls;
	struct device *device;
	unsigned int irq0;
	unsigned int irq2;
	unsigned int irq11;
	wait_queue_head_t wait_head;
	atomic_t key_available;

	wait_queue_head_t rwait_head;
	int key_val;
};

static struct key_t g_key={
	.key_available = ATOMIC_INIT(1),
	.irq0 = IRQ_EINT0,
	.irq2 = IRQ_EINT2,
	.irq11 = IRQ_EINT11,
	.key_val = 0,
};

irqreturn_t key_irq_handle(int irqno, void *arg)
{
	int val;
	switch(irqno)
	{
		case IRQ_EINT0:
			val = 1;
			break;
		case IRQ_EINT2:
			val = 2;
			break;
		case IRQ_EINT11:
			val = 3;
			break;
		default:
			val = 0;
	}

	g_key.key_val = val;
	
	wake_up(&g_key.rwait_head); // 唤醒读队列
	
	return IRQ_HANDLED;
}

static int key_is_available(void)
{
	if(!atomic_dec_and_test(&g_key.key_available))
	{
		atomic_inc(&g_key.key_available);
		return 0;
	}

	return 1;
}

static int key_open(struct inode *inode, struct file *filep)
{
	int ret;

	if(!atomic_dec_and_test(&g_key.key_available))
	{
		atomic_inc(&g_key.key_available);
		if(filep->f_flags & O_NONBLOCK){
			printk("led is unavailable.\n");
			return -EBUSY;
		}
		wait_event_interruptible(g_key.wait_head, 1==key_is_available());
	}

	ret=request_irq(g_key.irq0, key_irq_handle,IRQF_TRIGGER_FALLING,"key1", NULL);
	if(ret<0){
		printk("request_irq %d error.\n",g_key.irq0);
		return ret;
	}
	ret=request_irq(g_key.irq2, key_irq_handle,IRQF_TRIGGER_FALLING,"key2", NULL);
	if(ret<0){
		printk("request_irq %d error.\n",g_key.irq2);
		goto err1;
	}
	ret=request_irq(g_key.irq11, key_irq_handle,IRQF_TRIGGER_FALLING,"key3", NULL);
	if(ret<0){
		printk("request_irq %d error.\n",g_key.irq11);
		goto err2;
	}
	init_waitqueue_head(&g_key.rwait_head); // 初始化读队列
	printk("key_open .\n");
	return 0;
	
err1:
	free_irq(g_key.irq2, NULL);
err2:
	free_irq(g_key.irq0, NULL);
	return ret;

}

static int key_release(struct inode *inode, struct file *filep)
{
	free_irq(g_key.irq0, NULL);
	free_irq(g_key.irq2, NULL);
	free_irq(g_key.irq11, NULL);
	atomic_inc(&g_key.key_available);
	wake_up(&g_key.wait_head); //唤醒等待队列
	return 0;
}

ssize_t key_read(struct file *filep, char __user *buffer, size_t size, loff_t *offset)
{
	char buf[16]={0};
	int ret;
	int val = g_key.key_val;
	if(val == 0)
	{
		if(filep->f_flags&O_NONBLOCK)
		{
			return -EBUSY;
		}
		wait_event_interruptible(g_key.rwait_head, 0 != g_key.key_val);
	}
	sprintf(buf,"%d",g_key.key_val);
	ret= copy_to_user(buffer,&buf,strlen(buf));
	g_key.key_val = 0;
	return ret;
}


static const struct file_operations fops={
	.owner = THIS_MODULE,
	.open = key_open,
	.release = key_release,
	.read = key_read,
};

static int key_device_create(struct key_t *key,int minor,int count,const struct file_operations *fops)
{
	int ret;
	key->minor = minor;
	key->count = count;
	ret = alloc_chrdev_region(&key->devno, key->minor, key->count,"key");
	if(ret!=0){
		printk(KERN_ERR "alloc_chrdev_region error.\n");
		return ret;
	}
	key->cdev = cdev_alloc();
	if(!key->cdev){
		printk("cdev_alloc error.\n");
		goto err1;
	}

	cdev_init(key->cdev,fops);
	key->cdev->owner = THIS_MODULE;

	ret = cdev_add(key->cdev,key->devno,key->count);
	if(ret){
		printk("cdev_add error.\n");
		goto err2;
	}

	key->cls = class_create(THIS_MODULE, "led");
	if(IS_ERR(key->cls)){
		printk("class_create error.\n");
		ret = PTR_ERR(key->cls);
		goto err3;
	}
	key->device = device_create(key->cls,NULL,key->devno, NULL,"key");
	if(IS_ERR(key->device)){
		printk("device_create error,\n");
		ret = PTR_ERR(key->device);
		goto err4;
	}
	
	init_waitqueue_head(&key->wait_head);
	return 0;
err4:
	class_destroy(key->cls);
err3:
	cdev_del(key->cdev);
err2:
	kfree(key->cdev);
err1:
	unregister_chrdev_region(key->devno, key->count);
	return ret;

}

static void key_device_destroy(void)
{
	device_destroy(g_key.cls, g_key.devno);
	class_destroy(g_key.cls);
	cdev_del(g_key.cdev);
	kfree(g_key.cdev);
	unregister_chrdev_region(g_key.devno, g_key.count);
}


static int __init gkey_init(void)
{
	int ret;
	ret = key_device_create(&g_key,0,1,&fops);
	return ret;
}

static void __exit gkey_exit(void)
{
	key_device_destroy();
}

module_init(gkey_init);
module_exit(gkey_exit);

MODULE_LICENSE("GPL");


