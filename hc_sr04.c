#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/ktime.h>
#include <linux/jiffies.h>  // For timeout handling

#define DEVICE_NAME "rc_hs04"
#define TRIGGER_PIN 20
#define ECHO_PIN 21

static int major_number;
static struct class *rc_hs04_class = NULL;
static struct device *rc_hs04_device = NULL;

static int device_open(struct inode *inode, struct file *file) {
    return 0;
}

static int device_release(struct inode *inode, struct file *file) {
    return 0;
}

static ssize_t device_read(struct file *file, char __user *buffer, size_t len, loff_t *offset) {
    ktime_t start, end;
    s64 time_ns;
    s64 distance_cm;
    char msg[256];
    unsigned long timeout;
    
    // Trigger signal
    gpio_set_value(TRIGGER_PIN, 0);
    udelay(2);
    gpio_set_value(TRIGGER_PIN, 1);
    udelay(10);
    gpio_set_value(TRIGGER_PIN, 0);

    // Wait for ECHO signal with timeout
    timeout = jiffies + msecs_to_jiffies(200); // 200 ms timeout

    while (!gpio_get_value(ECHO_PIN)) {
        if (time_after(jiffies, timeout)) {
            printk(KERN_ALERT "ECHO 信號超時\n");
            return -ETIMEDOUT;
        }
        udelay(10);  // Small delay to prevent busy waiting from consuming too much CPU
    }
    start = ktime_get();

    while (gpio_get_value(ECHO_PIN)) {
        if (time_after(jiffies, timeout)) {
            printk(KERN_ALERT "ECHO 信號超時\n");
            return -ETIMEDOUT;
        }
        udelay(10);  // Small delay to prevent busy waiting from consuming too much CPU
    }
    end = ktime_get();

    time_ns = ktime_to_ns(ktime_sub(end, start));
    // Convert time to distance in cm
    distance_cm = (time_ns / 2) / 34300;

   

 if (copy_to_user(buffer, &distance_cm, sizeof(distance_cm))) {
        return -EFAULT;
    }

    return sizeof(distance_cm);
}
   

static struct file_operations fops = {
    .open = device_open,
    .release = device_release,
    .read = device_read,
};

static int __init rc_hs04_init(void) {
    // Request GPIO pins
    if (!gpio_is_valid(TRIGGER_PIN) || !gpio_is_valid(ECHO_PIN)) {
        printk(KERN_ALERT "無效的 GPIO 引腳\n");
        return -ENODEV;
    }

    gpio_request(TRIGGER_PIN, "Trigger");
    gpio_request(ECHO_PIN, "Echo");
    gpio_direction_output(TRIGGER_PIN, 0);
    gpio_direction_input(ECHO_PIN);

    // Register character device
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ALERT "無法註冊字符設備\n");
        gpio_free(TRIGGER_PIN);
        gpio_free(ECHO_PIN);
        return major_number;
    }

    // Create device class
    rc_hs04_class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(rc_hs04_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        gpio_free(TRIGGER_PIN);
        gpio_free(ECHO_PIN);
        printk(KERN_ALERT "無法創建設備類別\n");
        return PTR_ERR(rc_hs04_class);
    }

    // Create device
    rc_hs04_device = device_create(rc_hs04_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(rc_hs04_device)) {
        class_destroy(rc_hs04_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        gpio_free(TRIGGER_PIN);
        gpio_free(ECHO_PIN);
        printk(KERN_ALERT "無法創建設備\n");
        return PTR_ERR(rc_hs04_device);
    }

    printk(KERN_INFO "RC_HS04 模組初始化完成\n");
    return 0;
}

static void __exit rc_hs04_exit(void) {
    device_destroy(rc_hs04_class, MKDEV(major_number, 0));
    class_unregister(rc_hs04_class);
    class_destroy(rc_hs04_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    gpio_free(TRIGGER_PIN);
    gpio_free(ECHO_PIN);
    printk(KERN_INFO "RC_HS04 模組已卸載\n");
}

module_init(rc_hs04_init);
module_exit(rc_hs04_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ting");
MODULE_DESCRIPTION("RC_HS04 超音波距離感測器驅動模組");
