#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
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
	{ 0xfa985410, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x18e90764, __VMLINUX_SYMBOL_STR(blk_cleanup_queue) },
	{ 0x97726489, __VMLINUX_SYMBOL_STR(del_gendisk) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0xb5a459dc, __VMLINUX_SYMBOL_STR(unregister_blkdev) },
	{ 0x967825dd, __VMLINUX_SYMBOL_STR(add_disk) },
	{ 0x7f1b169b, __VMLINUX_SYMBOL_STR(blk_queue_make_request) },
	{ 0xf47339a5, __VMLINUX_SYMBOL_STR(put_disk) },
	{ 0x23c21e35, __VMLINUX_SYMBOL_STR(blk_alloc_queue) },
	{ 0xba02da45, __VMLINUX_SYMBOL_STR(alloc_disk) },
	{ 0x71a50dbc, __VMLINUX_SYMBOL_STR(register_blkdev) },
	{ 0x76f6c5ef, __VMLINUX_SYMBOL_STR(kmalloc_order) },
	{ 0x1e047854, __VMLINUX_SYMBOL_STR(warn_slowpath_fmt) },
	{ 0xb5ac343b, __VMLINUX_SYMBOL_STR(bio_endio) },
	{ 0x676bbc0f, __VMLINUX_SYMBOL_STR(_set_bit) },
	{ 0x9d669763, __VMLINUX_SYMBOL_STR(memcpy) },
	{ 0xdc9ee944, __VMLINUX_SYMBOL_STR(page_address) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0xefd6cf06, __VMLINUX_SYMBOL_STR(__aeabi_unwind_cpp_pr0) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "97B8D98B759CF7B9DE6C2E8");
