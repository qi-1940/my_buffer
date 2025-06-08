#include <linux/init.h>       // 包含初始化宏
#include <linux/module.h>     // 包含模块相关宏
#include <linux/kernel.h>      // 包含内核打印函数
#include <linux/fs.h>          // 包含文件操作结构体
#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/kfifo.h> 
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>    // proc 文件系统支持

#define DEVICE_NAME "my_buffer"
#define CLASS_NAME "my_buffer_class"
#define PROC_FILE_NAME "my_buffer_status"
#define BUFFER_SIZE 8

// 定义缓冲区
DEFINE_KFIFO(fifo_buffer, char, BUFFER_SIZE);

// 锁的声明
static DEFINE_SPINLOCK(io_lock);        // 保护IO操作
static DEFINE_SPINLOCK(proc_lock);      // 保护proc数据

// 字符设备相关
static struct cdev my_buffer_cdev;
static dev_t my_buffer_dev_t;
static struct class *my_class;
static struct proc_dir_entry* my_buffer_status_ptr;

// 等待队列
static wait_queue_head_t ReadyQueue;

// 当前缓冲区长度
static int current_len;

static int my_buffer_open(struct inode *inode, struct file *file)
{
	printk("my_buffer_open is called. \n");
	return 0;
}

static ssize_t my_buffer_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
    unsigned long flags;
    char kernel_buf[BUFFER_SIZE];
    ssize_t ret = 0;
    
    if (size > BUFFER_SIZE) {
        return -EINVAL;
    }
    
    // 等待有足够的数据
    wait_event_interruptible(ReadyQueue, {
        unsigned long flags;
        int ret;
        spin_lock_irqsave(&io_lock, flags);
        ret = (current_len >= size);
        spin_unlock_irqrestore(&io_lock, flags);
        ret;
    });
    
    if (signal_pending(current)) {
        return -ERESTARTSYS;
    }
    
    // 执行读操作
    spin_lock_irqsave(&io_lock, flags);
    
    kfifo_out(&fifo_buffer, kernel_buf, size);
    if (copy_to_user(buf, kernel_buf, size)) {
        ret = -EFAULT;
        goto out;
    }
    
    current_len = kfifo_len(&fifo_buffer);
    
    ret = size;
    
out:
    spin_unlock_irqrestore(&io_lock, flags);
    wake_up_interruptible(&ReadyQueue);
    return ret;
}

static ssize_t my_buffer_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
    unsigned long flags;
    char kernel_buf[BUFFER_SIZE];
    ssize_t ret = 0;
    
    if (size > BUFFER_SIZE) {
        return -EINVAL;
    }
    
    // 等待有足够的空间
    wait_event_interruptible(ReadyQueue, {
        unsigned long flags;
        int ret;
        spin_lock_irqsave(&io_lock, flags);
        ret = (current_len + size <= BUFFER_SIZE);
        spin_unlock_irqrestore(&io_lock, flags);
        ret;
    });
    
    if (signal_pending(current)) {
        return -ERESTARTSYS;
    }
    
    // 执行写操作
    spin_lock_irqsave(&io_lock, flags);
    
    if (copy_from_user(kernel_buf, buf, size)) {
        ret = -EFAULT;
        goto out;
    }
    
    kfifo_in(&fifo_buffer, kernel_buf, size);
    current_len = kfifo_len(&fifo_buffer);
    
    ret = size;
    
out:
    spin_unlock_irqrestore(&io_lock, flags);
    wake_up_interruptible(&ReadyQueue);
    return ret;
}

// 文件操作结构体
static struct file_operations my_buffer_fops = {
    .owner = THIS_MODULE,
    .open = my_buffer_open,
    .read = my_buffer_read,
    .write = my_buffer_write,
};

static ssize_t proc_show(struct file *file, char __user *buf, size_t count, loff_t *pos) {
    char *kbuf;
    int len = 0;
    unsigned long flags;
    struct wait_queue_entry *wq_entry;
    
    kbuf = kzalloc(PAGE_SIZE, GFP_KERNEL);
    if (!kbuf) {
        return -ENOMEM;
    }
    
    spin_lock_irqsave(&io_lock, flags);
    
    // 输出缓冲区内容
    len += scnprintf(kbuf + len, PAGE_SIZE - len, "Buffer Content: ");
    char temp_buf[BUFFER_SIZE];
    int temp_len = kfifo_out(&fifo_buffer, temp_buf, BUFFER_SIZE);
    for (int i = 0; i < temp_len; i++) {
        len += scnprintf(kbuf + len, PAGE_SIZE - len, "%c ", temp_buf[i]);
    }
    // 将数据放回缓冲区
    kfifo_in(&fifo_buffer, temp_buf, temp_len);
    len += scnprintf(kbuf + len, PAGE_SIZE - len, "\n");
    
    // 输出当前数据长度
    len += scnprintf(kbuf + len, PAGE_SIZE - len, 
        "Current Data Length: %d\n", current_len);
    
    // 输出等待队列中的进程信息
    len += scnprintf(kbuf + len, PAGE_SIZE - len, "Blocked Processes:\n");
    list_for_each_entry(wq_entry, &ReadyQueue.head, entry) {
        if (wq_entry->private) {
            struct task_struct *task = (struct task_struct *)wq_entry->private;
            len += scnprintf(kbuf + len, PAGE_SIZE - len,
                "PID: %d, Name: %s\n", task->pid, task->comm);
        }
    }
    
    spin_unlock_irqrestore(&io_lock, flags);
    
    len = simple_read_from_buffer(buf, count, pos, kbuf, len);
    kfree(kbuf);
    return len;
}

static struct proc_ops proc_fops = {
    .proc_read = proc_show,
};

static int __init my_buffer_init(void)
{
    // 创建proc文件（内核4.0+推荐方式）
    my_buffer_status_ptr = proc_create(PROC_FILE_NAME, 0444, NULL, &proc_fops);

	int ret;

    // 1. 分配设备号
    ret = alloc_chrdev_region(&my_buffer_dev_t, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ERR "Alloc dev failed\n");
        return ret;
    }

    // 2. 初始化并注册字符设备
    cdev_init(&my_buffer_cdev, &my_buffer_fops);
    my_buffer_cdev.owner = THIS_MODULE;
    ret = cdev_add(&my_buffer_cdev, my_buffer_dev_t, 1);
    if (ret < 0) {
        unregister_chrdev_region(my_buffer_dev_t, 1);
        printk(KERN_ERR "cdev_add failed\n");
        return ret;
    }

    // 3. 创建设备类和节点
    my_class = class_create(CLASS_NAME);
    if (IS_ERR(my_class)) {
        cdev_del(&my_buffer_cdev);
        unregister_chrdev_region(my_buffer_dev_t, 1);
        printk(KERN_ERR "class_create failed\n");
        return PTR_ERR(my_class);
    }

    device_create(my_class, NULL, my_buffer_dev_t, NULL, DEVICE_NAME);
    printk(KERN_INFO "Device created: Major=%d, Minor=%d\n",
           MAJOR(my_buffer_dev_t), MINOR(my_buffer_dev_t));

    init_waitqueue_head(&ReadyQueue);
    kfifo_reset(&fifo_buffer);

    current_len = 0;

    return 0;
}
 
static void __exit my_buffer_exit(void)
{
    remove_proc_entry(PROC_FILE_NAME, NULL);
	// 删除设备节点和类
    device_destroy(my_class, my_buffer_dev_t);
    class_destroy(my_class);

    // 注销字符设备
    cdev_del(&my_buffer_cdev);
    unregister_chrdev_region(my_buffer_dev_t, 1);
}
 
MODULE_LICENSE("GPL");
module_init(my_buffer_init);
module_exit(my_buffer_exit);
