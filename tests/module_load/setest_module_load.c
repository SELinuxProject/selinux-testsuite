#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

static int __init setest_module_load_init(void)
{
	pr_info("INIT - setest_module_load\n");
	return 0;
}

static void __exit setest_module_load_exit(void)
{
	pr_info("EXIT - setest_module_load\n");
}

module_init(setest_module_load_init);
module_exit(setest_module_load_exit);
MODULE_LICENSE("GPL");
