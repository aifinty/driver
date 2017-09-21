/*
*	驱动的参数传递，通常用于对模块参数的配置
*	by ztm 2017-9-21
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>

int num;
char *name = NULL;
int array[5];

module_param(num,int,S_IRUGO);
module_param(name,charp,S_IRUGO);
module_param_array(array,int ,NULL,S_IRUGO);

static int __init xx_init(void)
{
	int i;
	printk("num=%d\n",num);
	printk("name=%s\n",name);
	for(i=0;i<ARRAY_SIZE(array);i++)
	{
		printk("array[%d]=%d\n",i,array[i]);
	}
	return 0;
}

static void __exit xx_exit(void)
{
	printk("Good by\n");
}

module_init(xx_init);
module_exit(xx_exit);

MODULE_LICENSE("GPL");

/*
*	使用示例： insmod module_param.ko mun=5 name="ZTM" array=1,2,3,4,5
*/
