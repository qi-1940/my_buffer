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
#include <linux/seq_file.h>   // seq_file 接口（安全输出大文件）

#define DEVICE_NAME "my_buffer"
#define CLASS_NAME "my_buffer_class"
#define PROC_FILE_NAME "my_buffer_status"

DEFINE_KFIFO(fifo_buffer, char, 8);

// 锁的静态声明与初始化
//static DEFINE_SPINLOCK(fifo_lock);
static DEFINE_SPINLOCK(proc_lock);
static DEFINE_SPINLOCK(io_lock);

//字符设备相关
static struct cdev my_buffer_cdev;
static dev_t my_buffer_dev_t;
static struct class *my_class;

// 为读/写分别声明等待队列
static wait_queue_head_t ReadyQueue;

// 条件变量声明
static int current_len; 

static struct {                            // 定义一个匿名结构体，并声明为static（仅当前文件可见）
    char buffer[8];                     // 固定大小的缓冲区
    size_t data_len;                       // 当前缓冲区中有效数据的长度（字节数）
    int readers_blocked;                   // 正在等待（阻塞）读取数据的进程数量
    spinlock_t lock;                       // 自旋锁，用于保护对共享数据（buffer/data_len等）的并发访问
} my_buffer_proc_data = {                             // 定义结构体变量 drv_data，并进行初始化
    .lock = __SPIN_LOCK_UNLOCKED(my_buffer_proc_data.lock), // 初始化自旋锁为未锁定状态
};

static void show_kfifo_contents(void) {
    char buffer[8]; // 临时缓冲区，大小与 kfifo 容量一致
    unsigned long flags;
    //spin_lock_irqsave(&fifo_show_lock, flags);
    // 复制数据到临时缓冲区（不移动读指针）
    kfifo_out_peek(&fifo_buffer, buffer, 8);
    
    // 打印字符
    for (int i = 0; i < 8; i++) {
        printk(KERN_CONT "%c ", buffer[i]); // 连续输出字符
    }
    printk(KERN_CONT "\n");
    //spin_unlock_irqrestore(&fifo_show_lock, flags); 
}

static int my_buffer_open(struct inode *inode, struct file *file)
{
	printk("my_buffer_open is called. \n");
	return 0;
}

static ssize_t my_buffer_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
    DEFINE_WAIT(wait);  // 定义等待项
    unsigned long flags_wait;
    unsigned long flags;
    int ret = 0;

    spin_lock_irqsave(&io_lock, flags_wait);
    
    // 等待数据就绪（可中断）
    while (current_len<size) {
        prepare_to_wait(&ReadyQueue, &wait, TASK_INTERRUPTIBLE);
        spin_unlock_irqrestore(&io_lock, flags_wait);
        
        // 让出CPU，直到被唤醒或信号中断
        schedule();
        
        // 检查是否被信号中断
        if (signal_pending(current)) {
            ret = -ERESTARTSYS;
            break;
        }
        spin_lock_irqsave(&io_lock, flags_wait);
    }

    if (ret == 0) {
        //spin_lock_irqsave(&fifo_lock, flags); 
        //printk("my_buffer_read is called. \n");
        //printk("current_len:%d \n",current_len);
        //show_kfifo_contents();
        char kernel_buf[8];
        kfifo_out(&fifo_buffer, kernel_buf, size);
        // 将数据从内核空间拷贝到用户空间
        if (copy_to_user(buf, kernel_buf, size)) {
            return -EFAULT; // 拷贝失败
        }
        //show_kfifo_contents();
        current_len=kfifo_len(&fifo_buffer);
        //spin_unlock_irqrestore(&fifo_lock, flags);
        wake_up_interruptible_nr(&ReadyQueue,1);
    }

    finish_wait(&ReadyQueue, &wait);
    spin_unlock_irqrestore(&io_lock, flags_wait);
    return ret;
}

static ssize_t my_buffer_write (struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
    DEFINE_WAIT(wait);
    unsigned long flags_wait;
    unsigned long flags;
    int ret = 0;

    spin_lock_irqsave(&io_lock, flags_wait);
    // 等待缓冲区空间（可中断）
    while (size>(8-current_len)) {
        prepare_to_wait(&ReadyQueue, &wait, TASK_INTERRUPTIBLE);
        spin_unlock_irqrestore(&io_lock, flags_wait);
        
        schedule();
        
        if (signal_pending(current)) {
            ret = -ERESTARTSYS;
            break;
        }
        
        spin_lock_irqsave(&io_lock, flags_wait);
    }

    if (ret == 0) {
        //spin_lock_irqsave(&fifo_lock, flags); //fifo_lock**********************
        //printk("my_buffer_write is called. \n");
        //printk("current_len:%d \n",current_len);
        //显示缓冲区
        //show_kfifo_contents();

        //从用户空间获取要写入的数据
        char kernel_buf[8];
        if (copy_from_user(kernel_buf, buf, size))
            return -EFAULT;

        //把数据写到kfifo中
        unsigned int avail = kfifo_avail(&fifo_buffer);
        kfifo_in(&fifo_buffer,kernel_buf,size);
        current_len=kfifo_len(&fifo_buffer);
        //spin_unlock_irqrestore(&fifo_lock, flags);//fifo_unlock***********************
        wake_up_interruptible_nr(&ReadyQueue,1);
    }

    finish_wait(&ReadyQueue, &wait);
    spin_unlock_irqrestore(&io_lock, flags_wait);
    return ret;
}

// 定义 proc_ops
static const struct proc_ops my_buffer_proc_ops = {
    .proc_open    = my_buffer_open,   
    .proc_read    = my_buffer_read,        
    .proc_write = my_buffer_write,
};
 
static int __init my_buffer_init(void)
{
    // 创建proc文件（内核4.0+推荐方式）
    proc_create(PROC_FILE_NAME, 0444, NULL, &my_buffer_proc_ops);
    
    // 初始化统计信息
    memset(&drv_stats, 0, sizeof(drv_stats));

	int ret;

    // 1. 分配设备号
    ret = alloc_chrdev_region(&my_buffer_dev_t, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ERR "Alloc dev failed\n");
        return ret;
    }

    // 2. 初始化并注册字符设备
    cdev_init(&my_buffer_cdev, &my_buffer_ops);
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
