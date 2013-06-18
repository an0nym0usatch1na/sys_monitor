#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = KBUILD_MODNAME,
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
 .arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xaf18b453, "module_layout" },
	{ 0x7fbd3a04, "d_path" },
	{ 0x8fca5c84, "cdev_del" },
	{ 0x12da5bb2, "__kmalloc" },
	{ 0x43540ca9, "cdev_init" },
	{ 0xf9a482f9, "msleep" },
	{ 0x58db3f5, "up_read" },
	{ 0x92f0ef51, "mem_map" },
	{ 0xabd0c91c, "rtc_time_to_tm" },
	{ 0x67c2fa54, "__copy_to_user" },
	{ 0x2e5810c6, "__aeabi_unwind_cpp_pr1" },
	{ 0x97255bdf, "strlen" },
	{ 0x34184afe, "current_kernel_time" },
	{ 0xc5ae0182, "malloc_sizes" },
	{ 0x4fe38dbd, "down_interruptible" },
	{ 0x2bb3c0a5, "device_destroy" },
	{ 0x7485e15e, "unregister_chrdev_region" },
	{ 0x91715312, "sprintf" },
	{ 0x6ccf7bd7, "__pv_phys_offset" },
	{ 0x4e748dcd, "down_read" },
	{ 0x4aea4791, "__strncpy_from_user" },
	{ 0xfa2a45e, "__memzero" },
	{ 0x5f754e5a, "memset" },
	{ 0x27e1a049, "printk" },
	{ 0x7714684, "get_task_mm" },
	{ 0xf938af06, "device_create" },
	{ 0x53ba4e7c, "up_write" },
	{ 0x7bf30fdc, "down_write" },
	{ 0x61651be, "strcat" },
	{ 0x77febcbb, "cdev_add" },
	{ 0x9f984513, "strrchr" },
	{ 0x93fca811, "__get_free_pages" },
	{ 0x10247c9d, "get_user_pages" },
	{ 0x16a864ec, "kmem_cache_alloc_trace" },
	{ 0x7afa89fc, "vsnprintf" },
	{ 0x4302d0eb, "free_pages" },
	{ 0x4f68e5c9, "do_gettimeofday" },
	{ 0x37a0cba, "kfree" },
	{ 0x9d669763, "memcpy" },
	{ 0x364b3fff, "up" },
	{ 0xee7c1035, "put_page" },
	{ 0xc94224e, "class_destroy" },
	{ 0xefd6cf06, "__aeabi_unwind_cpp_pr0" },
	{ 0xc6e30372, "__class_create" },
	{ 0x796d56bd, "__init_rwsem" },
	{ 0x29537c9e, "alloc_chrdev_region" },
	{ 0xe914e41e, "strcpy" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

