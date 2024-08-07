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
	{ 0xab5c8cb5, __VMLINUX_SYMBOL_STR(i2c_del_driver) },
	{ 0x19bdcb34, __VMLINUX_SYMBOL_STR(i2c_register_driver) },
	{ 0xe3c07ed7, __VMLINUX_SYMBOL_STR(input_register_device) },
	{ 0x389329b3, __VMLINUX_SYMBOL_STR(gpiod_set_raw_value) },
	{ 0x3089e8ae, __VMLINUX_SYMBOL_STR(gpio_to_desc) },
	{ 0xf9a482f9, __VMLINUX_SYMBOL_STR(msleep) },
	{ 0x63961a83, __VMLINUX_SYMBOL_STR(dev_err) },
	{ 0x1352f55c, __VMLINUX_SYMBOL_STR(devm_gpio_request_one) },
	{ 0xbedc9d6d, __VMLINUX_SYMBOL_STR(input_mt_init_slots) },
	{ 0x1e8b2bb, __VMLINUX_SYMBOL_STR(input_set_abs_params) },
	{ 0x34fff4c2, __VMLINUX_SYMBOL_STR(devm_input_allocate_device) },
	{ 0x2770f6b8, __VMLINUX_SYMBOL_STR(devm_request_threaded_irq) },
	{ 0xde6fb46c, __VMLINUX_SYMBOL_STR(of_get_named_gpio_flags) },
	{ 0x1005a311, __VMLINUX_SYMBOL_STR(input_mt_report_pointer_emulation) },
	{ 0xddaa6aba, __VMLINUX_SYMBOL_STR(input_mt_report_slot_state) },
	{ 0x8e117d54, __VMLINUX_SYMBOL_STR(input_event) },
	{ 0xfa2a45e, __VMLINUX_SYMBOL_STR(__memzero) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x243f207d, __VMLINUX_SYMBOL_STR(i2c_transfer) },
	{ 0xefd6cf06, __VMLINUX_SYMBOL_STR(__aeabi_unwind_cpp_pr0) },
	{ 0xf8a905d3, __VMLINUX_SYMBOL_STR(input_unregister_device) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "085BBFABEF38393672EC154");
