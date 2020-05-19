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

MODULE_INFO(depends, "vop_bus,scif_bus,cosm_bus");

MODULE_ALIAS("pci:v00008086d00002260sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d00002262sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d00002263sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d00002265sv*sd*bc*sc*i*");

MODULE_INFO(srcversion, "6E13CD631C8D4E1EAC79A07");
