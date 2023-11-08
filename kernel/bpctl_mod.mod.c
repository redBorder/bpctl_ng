#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
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

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x3af7223e, "module_layout" },
	{ 0x6bc3fbc0, "__unregister_chrdev" },
	{ 0xeb233a45, "__kmalloc" },
	{ 0xf9a482f9, "msleep" },
	{ 0x349cba85, "strchr" },
	{ 0x33267c2e, "single_open" },
	{ 0x1ed8b3cb, "i2c_smbus_read_i2c_block_data" },
	{ 0xdf0256ec, "single_release" },
	{ 0x6bd0e573, "down_interruptible" },
	{ 0xf5f7945b, "seq_printf" },
	{ 0xd2da1048, "register_netdevice_notifier" },
	{ 0x83ba136e, "remove_proc_entry" },
	{ 0x2a4094da, "__register_chrdev" },
	{ 0xeae3dfd6, "__const_udelay" },
	{ 0x78534f62, "init_timer_key" },
	{ 0x4629334c, "__preempt_count" },
	{ 0x3c3ff9fd, "sprintf" },
	{ 0xa35a9cc0, "seq_read" },
	{ 0x15ba50a6, "jiffies" },
	{ 0x9d0d6206, "unregister_netdevice_notifier" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0xcd3c7696, "del_timer_sync" },
	{ 0xf9ca2eb4, "kstrtoint_from_user" },
	{ 0x5de4a7c6, "pci_get_subsys" },
	{ 0x591c81b7, "proc_mkdir" },
	{ 0xd35cce70, "_raw_spin_unlock_irqrestore" },
	{ 0xbcab6ee6, "sscanf" },
	{ 0xde80cd09, "ioremap" },
	{ 0x3cf85989, "mod_timer" },
	{ 0x28bbb0e9, "init_net" },
	{ 0xac1a55be, "unregister_reboot_notifier" },
	{ 0xa916b694, "strnlen" },
	{ 0x3517383e, "register_reboot_notifier" },
	{ 0xd0da656b, "__stack_chk_fail" },
	{ 0x92997ed8, "_printk" },
	{ 0x65487097, "__x86_indirect_thunk_rax" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0xcbd4898c, "fortify_panic" },
	{ 0x34db050b, "_raw_spin_lock_irqsave" },
	{ 0x9e4e29e2, "remove_proc_subtree" },
	{ 0xaa101e4c, "proc_create_data" },
	{ 0x42cd7ed2, "seq_lseek" },
	{ 0x37a0cba, "kfree" },
	{ 0xedc03953, "iounmap" },
	{ 0xcf2a6966, "up" },
	{ 0x927152bb, "i2c_smbus_write_i2c_block_data" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0x9e7d6bd0, "__udelay" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0x760a0f4f, "yield" },
	{ 0xe914e41e, "strcpy" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "AFE78E3D9FA2EB6478F9112");
MODULE_INFO(rhelversion, "9.2");
