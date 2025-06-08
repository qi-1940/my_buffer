#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xd4c28d75, "cdev_add" },
	{ 0xbec54aaa, "class_create" },
	{ 0x6b2a40b4, "device_create" },
	{ 0xd9a5ea54, "__init_waitqueue_head" },
	{ 0x239b51fb, "cdev_del" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0xfb0bf6ff, "remove_proc_entry" },
	{ 0xb4dc03ae, "device_destroy" },
	{ 0xea7ed1b1, "class_destroy" },
	{ 0xf1b37f6e, "kmalloc_caches" },
	{ 0xad2c94c1, "__kmalloc_cache_noprof" },
	{ 0x96848186, "scnprintf" },
	{ 0x619cb7dd, "simple_read_from_buffer" },
	{ 0x37a0cba, "kfree" },
	{ 0x98814e06, "const_pcpu_hot" },
	{ 0xad73041f, "autoremove_wake_function" },
	{ 0x1000e51, "schedule" },
	{ 0x92540fbf, "finish_wait" },
	{ 0xd5fd90f1, "prepare_to_wait" },
	{ 0xd35cce70, "_raw_spin_unlock_irqrestore" },
	{ 0x34db050b, "_raw_spin_lock_irqsave" },
	{ 0x13d0adf7, "__kfifo_out" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0x281823c5, "__kfifo_out_peek" },
	{ 0xe2964344, "__wake_up" },
	{ 0x7682ba4e, "__copy_overflow" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0xf23fcb99, "__kfifo_in" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x122c3a7e, "_printk" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x8a5dc39e, "proc_create" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x6e391b74, "cdev_init" },
	{ 0x3952d81, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "9866CCBD16AC0ED6B55841F");
