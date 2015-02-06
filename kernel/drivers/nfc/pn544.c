/*
 * Copyright (C) 2010 Trusted Logic S.A.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>  
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/nfc/pn544.h>
#include <mach/mt6575_gpio.h>
#include <mach/eint.h>
#include <cust_gpio_usage.h>
#include <cust_eint.h>
#include <linux/reboot.h>

#define MAX_BUFFER_SIZE	512
#undef PN544_VEN_REVERSE  
#define I2C_FIFO  8  //joey added,need to define according to baseband
#define I2C_timing         400 // i2c 100k - 400k
static int pn544_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int pn544_remove(struct i2c_client *client);
static int pn544_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
extern int register_reboot_notifier(struct notifier_block *nb);//joey
extern int unregister_reboot_notifier(struct notifier_block *nb);//joey

struct pn544_dev	{
	wait_queue_head_t	read_wq;
	struct mutex		read_mutex;
	struct i2c_client	*client;
	struct miscdevice	pn544_device;
	unsigned int 		ven_gpio;
	unsigned int 		firm_gpio;
	unsigned int		irq_gpio;
	bool			irq_enabled;
	spinlock_t		irq_enabled_lock;
};

static const struct i2c_device_id pn544_id[] = {
	{ "pn544", 0 },
	{ }
};
static struct i2c_driver pn544_driver = {
	.id_table	= pn544_id,
	.probe		= pn544_probe,
	.remove		= pn544_remove,
	.detect		= pn544_detect,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "pn544",
	},
};

static struct pn544_i2c_platform_data pn544_platform_data = {
   .irq_gpio = GPIO_NFC_EINT_PIN,
   .ven_gpio = GPIO_NFC_VEN_PIN,
   .firm_gpio = GPIO_NFC_FIRM_PIN,
};

struct pn544_dev *pn544_dev_ptr = NULL;

static int pn544_halt(struct notifier_block *nb, unsigned long event, void *buf) 
{

	switch (event) {
	case SYS_RESTART:	
				break;
	case SYS_HALT:		
				break;
	case SYS_POWER_OFF:		                    
		                      pr_info("%s power off\n", __func__);
       			              mt_set_gpio_out(GPIO_NFC_FIRM_PIN, GPIO_OUT_ZERO);
			              mt_set_gpio_out(GPIO_NFC_VEN_PIN, GPIO_OUT_ONE);
                                      msleep(50);
		                      pr_info("%s power on\n", __func__);									   
                                      mt_set_gpio_out(GPIO_NFC_FIRM_PIN, GPIO_OUT_ZERO);	
                		      mt_set_gpio_out(GPIO_NFC_VEN_PIN, GPIO_OUT_ZERO);
                                      msleep(50);						  
				break;
	default:		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block pn544_notifier = {
	.notifier_call = pn544_halt,
};

static void pn544_disable_irq(struct pn544_dev *pn544_dev)
{
	unsigned long flags;

	spin_lock_irqsave(&pn544_dev->irq_enabled_lock, flags);
	if (pn544_dev->irq_enabled) {	
		mt65xx_eint_mask(pn544_dev->client->irq);
		pn544_dev->irq_enabled = false;
	}
	spin_unlock_irqrestore(&pn544_dev->irq_enabled_lock, flags);
}

void pn544_dev_irq_handler(void)
{
	struct pn544_dev *pn544_dev = pn544_dev_ptr;

	pr_info("%s \n", __func__);
	pn544_disable_irq(pn544_dev);
	/* Wake up waiting readers */
	wake_up(&pn544_dev->read_wq);	
}

static ssize_t pn544_dev_read(struct file *filp, char __user *buf,
		size_t count, loff_t *offset)
{
	struct pn544_dev *pn544_dev = filp->private_data;
	int ret,num = 0;
	int i;
	int addr = 0;
	char rcv_tmp[MAX_BUFFER_SIZE];

	pr_info("%s : %d \n", __func__, count);

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	mutex_lock(&pn544_dev->read_mutex);

	if (!mt_get_gpio_in(pn544_dev->irq_gpio)) {
		if (filp->f_flags & O_NONBLOCK) {
                        pr_info("gotofail \n"); 
			ret = -EAGAIN;
			goto fail;
		}
		pn544_dev->irq_enabled = true;
		mt65xx_eint_unmask(pn544_dev->client->irq);
	        pr_info("%s : mt65xx_eint_unmask %d\n", __func__, pn544_dev->client->irq);
		ret = wait_event_interruptible(pn544_dev->read_wq,
				mt_get_gpio_in(pn544_dev->irq_gpio));
        	pr_info("%s : wait_event_interruptible ret %d\n", __func__, ret);
		pn544_disable_irq(pn544_dev);
		if (ret)			
			goto fail;		
	}

	while (count > I2C_FIFO) {
		ret = i2c_master_recv(pn544_dev->client, rcv_tmp + addr, I2C_FIFO);
                pr_info("%s: i2c_master_recv returned %d\n", __func__, ret);
	        if(ret < 0 ||ret > I2C_FIFO)
			goto fail; 							
		count -= I2C_FIFO;
		addr += I2C_FIFO;
		num += I2C_FIFO;
	}
	
	     ret = i2c_master_recv(pn544_dev->client, rcv_tmp+addr, count);
             num += ret;
	     pr_info("%s: i2c_master_recv returned %d\n", __func__, ret);		 
	     if(ret < 0 ||ret > count)
		 goto fail; 			
	
	mutex_unlock(&pn544_dev->read_mutex);		
	if (copy_to_user(buf, rcv_tmp, num)) {
		pr_warning("%s : failed to copy to user space\n", __func__);
		return -EFAULT;
	}
	 
	return num;

fail:
	mutex_unlock(&pn544_dev->read_mutex);
	if (ret < 0) {
		pr_err("%s: i2c_master_recv returned %d\n", __func__, ret);
		return ret;
	}
	if (ret > count) {
		pr_err("%s: received too many bytes from i2c (%d)\n",
			__func__, ret);
		return -EIO;
	}
	return ret;
}

static ssize_t pn544_dev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *offset)
{
	struct pn544_dev  *pn544_dev;
	int ret;
        char sent_tmp[MAX_BUFFER_SIZE];
	pr_info("%s : %d\n", __func__, count);
	pn544_dev = filp->private_data;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;
	if (copy_from_user(sent_tmp, buf, count)) {
		pr_err("%s : failed to copy from user space\n", __func__);
		return -EFAULT;
	}	
	ret = i2c_master_send(pn544_dev->client, sent_tmp, count);
	    pr_info("%s : Status %zu \n", __func__,ret);
	 if(ret != count) 
	  goto fail; 			
	
	return ret;

fail:			
          pr_err("%s : i2c_master_send returned %d\n", __func__, ret);
	  ret = -EIO;
	  return ret;
}

static int pn544_dev_open(struct inode *inode, struct file *filp)
{
	struct pn544_dev *pn544_dev = container_of(filp->private_data,
						struct pn544_dev,
						pn544_device);

	filp->private_data = pn544_dev;
	pn544_dev_ptr = pn544_dev;
	pr_info("%s : %d,%d, &pn544_dev_ptr=%p\n", __func__, imajor(inode), iminor(inode), pn544_dev_ptr);
	return 0;
}

static int pn544_dev_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{

	struct pn544_dev *pn544_dev = filp->private_data;
	switch (cmd) {
	case PN544_SET_PWR:
		if (arg == 2) {
			/* power on with firmware download (requires hw reset)
			 */
			pr_info("%s power on with firmware\n", __func__);		
			mt_set_gpio_out(pn544_dev->ven_gpio, GPIO_OUT_ZERO);
			mt_set_gpio_out(pn544_dev->firm_gpio, GPIO_OUT_ONE);
			msleep(10);
			mt_set_gpio_out(pn544_dev->ven_gpio, GPIO_OUT_ONE);
			msleep(10);
			mt_set_gpio_out(pn544_dev->ven_gpio, GPIO_OUT_ZERO);
			msleep(10);
		}
		else if (arg == 1) {
			/* power on */
			pr_info("%s power on \n", __func__);			
			mt_set_gpio_out(pn544_dev->firm_gpio, GPIO_OUT_ZERO);					
			mt_set_gpio_out(pn544_dev->ven_gpio, GPIO_OUT_ZERO);			
			msleep(10);
		} 
		else  if (arg == 0) {
			/* power off */
			pr_info("%s power off \n", __func__);
			mt_set_gpio_out(pn544_dev->firm_gpio, GPIO_OUT_ZERO);
			mt_set_gpio_out(pn544_dev->ven_gpio, GPIO_OUT_ONE);
			msleep(10);			      
		} 
		else {
			pr_err("%s bad arg %lu\n", __func__, arg);
			return -EINVAL;
		}
		break;
	default:
		pr_err("%s bad ioctl %u\n", __func__, cmd);
		return -EINVAL;
	}
	
}

static const struct file_operations pn544_dev_fops = {
	.owner	= THIS_MODULE,
	.llseek	= no_llseek,
	.read	= pn544_dev_read,
	.write	= pn544_dev_write,
	.open	= pn544_dev_open,
	.unlocked_ioctl  = pn544_dev_ioctl,  //joey
};

static int pn544_detect(struct i2c_client *client, int kind, struct i2c_board_info *info)
{
    pr_info("pn544_detect\n");
    strcpy(info->type, "pn544");
    return 0;
}

static int pn544_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret;
	struct pn544_i2c_platform_data *platform_data;
	struct pn544_dev *pn544_dev;      
	
	pr_info("pn544_probe\n");	
        client->timing = I2C_timing; //joey set i2c speed
	platform_data = &pn544_platform_data;

	if (platform_data == NULL) {
		pr_err("%s : nfc probe fail\n", __func__);
		return  -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s : need I2C_FUNC_I2C\n", __func__);
		return  -ENODEV;
	}

   /* irq_gpio */
	mt_set_gpio_mode(platform_data->irq_gpio, GPIO_NFC_EINT_PIN_M_EINT);
	mt_set_gpio_dir(platform_data->irq_gpio, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(platform_data->irq_gpio, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(platform_data->irq_gpio, GPIO_PULL_DOWN);
	/* ven_gpio */
	mt_set_gpio_mode(platform_data->ven_gpio, GPIO_NFC_VEN_PIN_M_GPIO);
	mt_set_gpio_dir(platform_data->ven_gpio, GPIO_DIR_OUT);
	/* firm_gpio */
	mt_set_gpio_mode(platform_data->firm_gpio, GPIO_NFC_FIRM_PIN_M_GPIO);
	mt_set_gpio_dir(platform_data->firm_gpio, GPIO_DIR_OUT);
	
	pn544_dev = kzalloc(sizeof(*pn544_dev), GFP_KERNEL);
	if (pn544_dev == NULL) {
		pr_err("pn544_dev: failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	pn544_dev->irq_gpio = platform_data->irq_gpio;
	pn544_dev->ven_gpio  = platform_data->ven_gpio;
	pn544_dev->firm_gpio  = platform_data->firm_gpio;
	pn544_dev->client   = client;

	/* init mutex and queues */
	init_waitqueue_head(&pn544_dev->read_wq);
	mutex_init(&pn544_dev->read_mutex);
	spin_lock_init(&pn544_dev->irq_enabled_lock);

	pn544_dev->pn544_device.minor = MISC_DYNAMIC_MINOR;
	pn544_dev->pn544_device.name = "pn544";
	pn544_dev->pn544_device.fops = &pn544_dev_fops;

	ret = misc_register(&pn544_dev->pn544_device);
	if (ret) {
		pr_err("%s : misc_register failed\n", __FILE__);
		goto err_misc_register;
	}


	/* request irq.  the irq is set whenever the chip has data available
	 * for reading.  it is cleared when all data has been read.
	 */
	client->irq = CUST_EINT_NFC_NUM;
	pr_info("%s : requesting IRQ %d\n", __func__, client->irq);
	pn544_dev->irq_enabled = true;
	mt65xx_eint_set_sens(CUST_EINT_NFC_NUM, CUST_EINT_NFC_SENSITIVE);
	mt65xx_eint_set_hw_debounce(CUST_EINT_NFC_NUM, CUST_EINT_NFC_DEBOUNCE_CN);
	mt65xx_eint_registration(CUST_EINT_NFC_NUM, CUST_EINT_NFC_DEBOUNCE_EN, CUST_EINT_NFC_POLARITY, pn544_dev_irq_handler, 0);
	mt65xx_eint_unmask(CUST_EINT_NFC_NUM);
	pn544_disable_irq(pn544_dev);
	i2c_set_clientdata(client, pn544_dev);       
	return 0;

err_request_irq_failed:
	misc_deregister(&pn544_dev->pn544_device);
err_misc_register:
	mutex_destroy(&pn544_dev->read_mutex);
	kfree(pn544_dev);
err_exit:
	gpio_free(platform_data->firm_gpio);
err_firm:
	gpio_free(platform_data->ven_gpio);
err_ven:
	gpio_free(platform_data->irq_gpio);
	return ret;
}

static int pn544_remove(struct i2c_client *client)
{
	struct pn544_dev *pn544_dev;
	
	pn544_dev = i2c_get_clientdata(client);
	free_irq(client->irq, pn544_dev);
	misc_deregister(&pn544_dev->pn544_device);
	mutex_destroy(&pn544_dev->read_mutex);
	gpio_free(pn544_dev->irq_gpio);
	gpio_free(pn544_dev->ven_gpio);
	gpio_free(pn544_dev->firm_gpio);
	kfree(pn544_dev);

	return 0;
}

/*
 * module load/unload record keeping
 */

static int __init pn544_dev_init(void)
{     
	pr_info("Loading pn544 driver\n");
        register_reboot_notifier(&pn544_notifier);//joey		
        return i2c_add_driver(&pn544_driver);
	
		
}
module_init(pn544_dev_init);

static void __exit pn544_dev_exit(void)
{
	pr_info("Unloading pn544 driver\n");
        unregister_reboot_notifier(&pn544_notifier);//joey		
        i2c_del_driver(&pn544_driver);
      
		
}
module_exit(pn544_dev_exit);

MODULE_AUTHOR("Sylvain Fonteneau");
MODULE_DESCRIPTION("NFC PN544 driver");
MODULE_LICENSE("GPL");






