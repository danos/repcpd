/**
 * @file main.c  PCP Server
 *
 * Copyright (C) 2010 Creytiv.com
 */

#include <getopt.h>
#include <signal.h>
#include <time.h>
#include <re.h>
#include <repcpd.h>
#include "pcpd.h"


static const char *configfile = "/etc/repcpd.conf";
static struct conf *conf;
static bool force_debug;
static time_t start_time;


static void signal_handler(int sig)
{
	info("caught signal %d\n", sig);

	switch (sig) {

	case SIGINT:
	case SIGTERM:
		re_cancel();
		break;

	default:
		info("unhandled signal %d\n", sig);
		break;
	}
}


static int module_handler(const struct pl *val, void *arg)
{
	struct pl *modpath = arg;
	char filepath[256];
	struct mod *mod;
	int err;

	if (val->p && val->l && (*val->p == '/'))
		(void)re_snprintf(filepath, sizeof(filepath), "%r", val);
	else
		(void)re_snprintf(filepath, sizeof(filepath), "%r/%r",
				  modpath, val);

	err = mod_load(&mod, filepath);
	if (err) {
		warning("can't load module %s (%m)\n",
			filepath, err);
		goto out;
	}

 out:
	return err;
}


struct conf *_conf(void)
{
	return conf;
}


uint32_t repcpd_epoch_time(void)
{
	time_t now;

	time(&now);

	return (uint32_t)(now - start_time);
}


static void usage(void)
{
	(void)re_fprintf(stderr, "usage: repcpd [-dhn] [-f <file>]\n");
	(void)re_fprintf(stderr, "\t-d         Turn on debugging\n");
	(void)re_fprintf(stderr, "\t-h         Show summary of options\n");
	(void)re_fprintf(stderr, "\t-n         Run in foreground\n");
	(void)re_fprintf(stderr, "\t-f <file>  Configuration file\n");
}


int main(int argc, char *argv[])
{
	bool daemon = true, dbg = false;
	int err = 0;
	struct pl opt;

	(void)sys_coredump_set(true);

	for (;;) {

		const int c = getopt(argc, argv, "dhnf:");
		if (0 > c)
			break;

		switch (c) {

		case 'd':
			force_debug = true;
			break;

		case 'f':
			configfile = optarg;
			break;

		case 'n':
			daemon = false;
			break;

		case '?':
			err = EINVAL;
			/*@fallthrough@*/
		case 'h':
			usage();
			return err;
		}
	}

	err = fd_setsize(4096);
	if (err) {
		warning("fd_setsize error: %m\n", err);
		goto out;
	}

	err = libre_init();
	if (err) {
		error("re init failed: %m\n", err);
		goto out;
	}

	/* configuration file */
	err = conf_alloc(&conf, configfile);
	if (err) {
		error("error loading configuration: %s: %m\n",
		      configfile, err);
		goto out;
	}

	/* debug config */
	(void)conf_get_bool(_conf(), "debug", &dbg);
	log_enable_debug(force_debug || dbg);

	(void)time(&start_time);

	/* udp */
	err = repcpd_udp_init();
	if (err)
		goto out;

	/* extaddr */
	err = repcpd_extaddr_init();
	if (err)
		goto out;

	/* daemon config */
	if (!conf_get(conf, "daemon", &opt) && !pl_strcasecmp(&opt, "no"))
		daemon = false;

	/* module config */
	if (conf_get(conf, "module_path", &opt))
		pl_set_str(&opt, ".");

	err = conf_apply(conf, "module", module_handler, &opt);
	if (err)
		goto out;

	/* daemon */
	if (daemon) {
		err = sys_daemon();
		if (err) {
			error("daemon error: %m\n", err);
			goto out;
		}

		log_enable_stderr(false);
	}

	info("PCP server ready.\n");

	/* main loop */
	err = re_main(signal_handler);

 out:
	info("PCP server terminated.\n");
	mod_close();
	repcpd_extaddr_close();
	repcpd_udp_close();
	conf = mem_deref(conf);

	libre_close();

	/* check for memory leaks */
	tmr_debug();
	mem_debug();

	return err;
}
