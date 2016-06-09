/**
 * @file syslog.c Syslog
 *
 * Copyright (C) 2010 Creytiv.com
 */

#include <syslog.h>
#include <re.h>
#include <repcpd.h>


static const int lmap[] = { LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERR };


static void log_handler(uint32_t level, const char *msg)
{
	syslog(lmap[MIN(level, ARRAY_SIZE(lmap)-1)], "%s", msg);
}


static struct log lg = {
	.h = log_handler,
};


static int module_init(void)
{
	uint32_t facility = LOG_DAEMON;

	conf_get_u32(_conf(), "syslog_facility", &facility);

	openlog("repcpd", LOG_NDELAY | LOG_PID, facility);

	log_register_handler(&lg);

	return 0;
}


static int module_close(void)
{
	log_unregister_handler(&lg);

	closelog();

	return 0;
}


EXPORT_SYM const struct mod_export DECL_EXPORTS(syslog) = {
	"syslog",
	"logger",
	module_init,
	module_close,
};
