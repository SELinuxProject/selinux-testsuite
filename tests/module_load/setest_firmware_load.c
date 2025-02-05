#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/firmware.h>

static int __init setest_firmware_request_init(void)
{
	const struct firmware *fw;
	int result;

	pr_info("INIT - setest_firmware_request\n");
	result = request_firmware(&fw, "dummy-firmware", NULL);
	if (result) {
		pr_err("request_firmware failed: %d\n", result);
		return result;
	}
	pr_info("request_firmware succeeded\n");
	release_firmware(fw);
	return 0;
}

static void __exit setest_firmware_request_exit(void)
{
	pr_info("EXIT - setest_firmware_request\n");
}

module_init(setest_firmware_request_init);
module_exit(setest_firmware_request_exit);
MODULE_LICENSE("GPL");