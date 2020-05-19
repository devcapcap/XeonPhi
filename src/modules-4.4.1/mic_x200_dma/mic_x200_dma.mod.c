#include <linux/build-salt.h>
#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(.gnu.linkonce.this_module) = {
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

MODULE_INFO(depends, "");

MODULE_ALIAS("pci:v00008086d00002264sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010B5d000087D0sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010B5d000087E0sv*sd*bc*sc*i*");

MODULE_INFO(srcversion, "54DF1D8F0A9ADE6290A48BB");
