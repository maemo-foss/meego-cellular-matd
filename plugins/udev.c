#include <libudev.h>
#include <stdlib.h>
#include <string.h>

#include <at_command.h>
#include <at_thread.h>
#include <at_log.h>

static at_error_t do_cbc (at_modem_t *m, void *data)
{
	(void)data;

	int canc = at_cancel_disable ();
	at_error_t ret = AT_OK;

	struct udev *udev = udev_new ();
	if (!udev) {
		ret = AT_ERROR;
		goto end;
	}

	struct udev_enumerate *en = udev_enumerate_new (udev);
	udev_enumerate_add_match_subsystem (en, "power_supply");
	udev_enumerate_add_match_sysattr (en, "type", "Battery");
	udev_enumerate_scan_devices (en);

	struct udev_list_entry *devs = udev_enumerate_get_list_entry (en);
	struct udev_list_entry *i;

	unsigned long total_charge = 0;
	unsigned long max_charge = 0;

	udev_list_entry_foreach (i, devs)
	{
		struct udev_device *d = udev_device_new_from_syspath (
				udev, udev_list_entry_get_name (i));
		const char *chrg = udev_device_get_sysattr_value (
				d, "charge_now");
		const char *chmx = udev_device_get_sysattr_value (
				d, "charge_full");

		if (chrg && chmx)
		{
			unsigned long charge = strtoul (chrg, NULL, 10);
			unsigned long max = strtoul (chmx, NULL, 10);

			if (charge > max)
				charge = max;

			total_charge += charge;
			max_charge += max;
		}

		udev_device_unref(d);
	}

	udev_enumerate_unref(en);
	udev_unref(udev);

	if (max_charge == 0)
		at_intermediate (m, "\r\n+CBC: 2,0");
	else
		at_intermediate (m, "\r\n+CBC: 0,%lu",
				 100 * total_charge / max_charge);

end:
	at_cancel_enable (canc);
	return ret;
}

static at_error_t list_cbc (at_modem_t *m, void *data)
{
	(void)data;

	at_intermediate (m, "\r\n+CBC: (0-2),(0-100)");

	return AT_OK;
}

static at_error_t handle_cbc (at_modem_t *m, const char *req, void *data)
{
	req += 4;
	req += strspn (req, " ");

	if (!*req)
		return do_cbc (m, data);

	if (*(req++) != '=')
		return AT_CME_EINVAL;
	req += strspn (req, " ");

	if (*(req++) != '?')
		return AT_CME_EINVAL;
	req += strspn (req, " ");

	if (*req)
		return AT_CME_EINVAL;

	return list_cbc (m, data);
}

void *at_plugin_register (at_commands_t *set)
{
	at_register (set, "+CBC", handle_cbc, NULL);

	return NULL;
}
