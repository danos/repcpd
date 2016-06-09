/**
 * @file log.c Logging
 *
 * Copyright (C) 2010 - 2016 Creytiv.com
 */

#include <re.h>
#include <repcpd.h>


static struct {
	struct list logl;
	bool debug;
	bool stder;
} lg = {
	.logl  = LIST_INIT,
	.debug = false,
	.stder = true
};


void log_register_handler(struct log *log)
{
	if (!log)
		return;

	list_append(&lg.logl, &log->le, log);
}


void log_unregister_handler(struct log *log)
{
	if (!log)
		return;

	list_unlink(&log->le);
}


void log_enable_debug(bool enable)
{
	lg.debug = enable;
}


void log_enable_stderr(bool enable)
{
	lg.stder = enable;
}


void vlog(enum log_level level, const char *fmt, va_list ap)
{
	char buf[4096];
	struct le *le;

	if (re_vsnprintf(buf, sizeof(buf), fmt, ap) < 0)
		return;

	if (lg.stder)
		(void)re_fprintf(stderr, "%s", buf);

	le = lg.logl.head;

	while (le) {

		struct log *log = le->data;
		le = le->next;

		if (log->h)
			log->h(level, buf);
	}
}


void loglv(enum log_level level, const char *fmt, ...)
{
	va_list ap;

	if ((DEBUG == level) && !lg.debug)
		return;

	va_start(ap, fmt);
	vlog(level, fmt, ap);
	va_end(ap);
}


void debug(const char *fmt, ...)
{
	va_list ap;

	if (!lg.debug)
		return;

	va_start(ap, fmt);
	vlog(DEBUG, fmt, ap);
	va_end(ap);
}


void info(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vlog(INFO, fmt, ap);
	va_end(ap);
}


void warning(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vlog(WARN, fmt, ap);
	va_end(ap);
}


void error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vlog(ERROR, fmt, ap);
	va_end(ap);
}
