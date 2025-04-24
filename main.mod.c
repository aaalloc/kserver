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
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x49cd25ed, "alloc_workqueue" },
	{ 0x6669a496, "kernel_accept" },
	{ 0x4c03a563, "random_kmalloc_seed" },
	{ 0xd9c18c81, "kmalloc_caches" },
	{ 0x7fc44b92, "__kmalloc_cache_noprof" },
	{ 0xc5b6f236, "queue_work_on" },
	{ 0x037a0cba, "kfree" },
	{ 0x122c3a7e, "_printk" },
	{ 0xb19bbf38, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "C9D368F83A0AE0226D639E8");
