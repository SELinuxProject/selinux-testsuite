#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

static int __init setest_module_request_init(void)
{
	int result;

	pr_info("INIT - setest_module_request\n");
	result = request_module_nowait("dummy-module");
	pr_info("request_module() returned: %d\n", result);
	return result;
}

static void __exit setest_module_request_exit(void)
{
	pr_info("EXIT - setest_module_request\n");
}

module_init(setest_module_request_init);
module_exit(setest_module_request_exit);
MODULE_LICENSE("GPL");
