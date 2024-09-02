
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/uaccess.h>  //copy_to/from_user()
#include <linux/gpio.h>     //GPIO
#include <linux/err.h>

#define PIN (26)
#define DEVICE_MAJOR 232
#define DEVICE_NAME "dht11"
#define LED_PIN (19)  // LED pin

typedef unsigned char U8;
unsigned char buf[6];
unsigned char check_flag;

int data_in(void)
{
    gpio_direction_input(PIN);
    return gpio_get_value(PIN);
}

void gpio_out(int pin, int value)
{
    gpio_direction_output(pin, value);
}

void temperature_read_data(void)
{
    int i, num;
    unsigned char flag = 0;
    unsigned char data = 0;

    gpio_out(PIN, 0);
    mdelay(30);
    gpio_out(PIN, 1);
    udelay(40);

    if (data_in() == 0)
    {
        while (!gpio_get_value(PIN))
        {
            udelay(5);
            if (++i > 20)
            {
                printk("error: timeout waiting for GPIO high\n");
                return;
            }
        }
        i = 0;
        while (gpio_get_value(PIN))
        {
            udelay(5);
            if (++i > 20)
            {
                printk("error: timeout waiting for GPIO low\n");
                return;
            }
        }
    }

    for (i = 0; i < 5; i++)
    {
        data = 0;
        for (num = 0; num < 8; num++)
        {
            int j = 0;
            while (!gpio_get_value(PIN))
            {
                udelay(10);
                if (++j > 40)
                    break;
            }
            flag = 0;
            udelay(28);
            if (gpio_get_value(PIN))
            {
                flag = 0x01;
            }
            j = 0;
            while (gpio_get_value(PIN))
            {
                udelay(10);
                if (++j > 60)
                    break;
            }
            data <<= 1;
            data |= flag;
        }
        buf[i] = data;
    }
    buf[5] = buf[0] + buf[1] + buf[2] + buf[3];
    if (buf[4] == buf[5])
    {
        check_flag = 0xff;
        printk("check pass\n");
        printk("temperature=%d.%d\n", buf[2], buf[3]);
    }
    else
    {
        check_flag = 0x0;
        printk("temperature check fail\n");
    }

    if(buf[2]>=30) 

    {
        gpio_out(LED_PIN, 1);
    }
    else
    {
        gpio_out(LED_PIN, 0);
    }
    

}

static ssize_t temperature_read(struct file *file, char *buffer, size_t size, loff_t *off)
{
    int ret;

    local_irq_disable();
    temperature_read_data();
    local_irq_enable();

    if (check_flag == 0xff)
    {
        ret = copy_to_user(buffer, buf, sizeof(buf));
        if (ret < 0)
        {
            printk("error: copy_to_user failed\n");
            return -EFAULT;
        }
          return sizeof(buf);
    }
    else
    {
        return 0;
    }
}

static int temperature_open(struct inode *inode, struct file *file)
{
    printk("open in kernel\n");
    return 0;
}

static int temperature_release(struct inode *inode, struct file *file)
{
    printk("humidity release\n");
    return 0;
}

static struct file_operations humidity_dev_fops = {
    .owner = THIS_MODULE,
    .open = temperature_open,
    .read = temperature_read,
    .release = temperature_release,
};

static struct class *dht_class;
static struct device *dht_device;

static int __init temperature_dev_init(void)
{
    int ret;

    ret = register_chrdev(DEVICE_MAJOR, DEVICE_NAME, &humidity_dev_fops);
    if (ret < 0)
    {
        printk(KERN_ERR "%s: registering device %s with major %d failed with %d\n",
               __func__, DEVICE_NAME, DEVICE_MAJOR, ret);
        return ret;
    }
    printk("DHT11 driver register success!\n");

    dht_class = class_create(THIS_MODULE, "dht11");
    if (IS_ERR(dht_class))
    {
        printk(KERN_ERR "Can't create class for %s\n", DEVICE_NAME);
        unregister_chrdev(DEVICE_MAJOR, DEVICE_NAME);
        return PTR_ERR(dht_class);
    }

    dht_device = device_create(dht_class, NULL, MKDEV(DEVICE_MAJOR, 0), NULL, DEVICE_NAME);
    if (IS_ERR(dht_device))
    {
        printk(KERN_ERR "Can't create device for %s\n", DEVICE_NAME);
        class_destroy(dht_class);
        unregister_chrdev(DEVICE_MAJOR, DEVICE_NAME);
        return PTR_ERR(dht_device);
    }
         gpio_request(LED_PIN, DEVICE_NAME);
    if ( gpio_request(PIN, DEVICE_NAME) < 0)
    {
        printk(KERN_ERR "Unable to get GPIO\n");
        device_destroy(dht_class, MKDEV(DEVICE_MAJOR, 0));
        class_destroy(dht_class);
        unregister_chrdev(DEVICE_MAJOR, DEVICE_NAME);
        return -EBUSY;
    }
gpio_direction_output(LED_PIN, 0);
    if (gpio_direction_output(PIN, 1) < 0)
    {
        printk(KERN_ERR "Unable to set GPIO direction\n");
        gpio_free(LED_PIN);
        gpio_free(PIN);
        device_destroy(dht_class, MKDEV(DEVICE_MAJOR, 0));
        class_destroy(dht_class);
        unregister_chrdev(DEVICE_MAJOR, DEVICE_NAME);
        return -EBUSY;
    }

    return 0;
}

static void __exit temperature_dev_exit(void)
{
    if (dht_device)
    {
        device_destroy(dht_class, MKDEV(DEVICE_MAJOR, 0));
    }
    if (dht_class)
    {
        class_destroy(dht_class);
    }
    gpio_out(LED_PIN, 0);
    gpio_free(LED_PIN);
    gpio_free(PIN);
    unregister_chrdev(DEVICE_MAJOR, DEVICE_NAME);
}

module_init(humidity_dev_init);
module_exit(humidity_dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("WWW");
