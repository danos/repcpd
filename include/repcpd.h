/**
 * @file repcpd.h Main interface to PCP server
 *
 * Copyright (C) 2010 - 2016 Creytiv.com
 */


#include <rew.h>


/*
 * Log
 */

enum log_level {
	DEBUG = 0,
	INFO,
	WARN,
	ERROR,
};

typedef void (log_h)(uint32_t level, const char *msg);

struct log {
	struct le le;
	log_h *h;
};

void log_register_handler(struct log *log);
void log_unregister_handler(struct log *log);
void log_enable_debug(bool enable);
void log_enable_stderr(bool enable);
void vlog(enum log_level level, const char *fmt, va_list ap);
void loglv(enum log_level level, const char *fmt, ...);
void debug(const char *fmt, ...);
void info(const char *fmt, ...);
void warning(const char *fmt, ...);
void error(const char *fmt, ...);


/*
 * Mapping-Table API
 */

struct mapping_table;

int mapping_table_alloc(struct mapping_table **tablep, const char *name);


struct mapping {
	struct le le;
	struct tmr tmr;
	enum pcp_opcode opcode;
	struct sa int_addr;  /* ADDR-part and PORT from 'map' (duplicated) */
	struct pcp_map map;

	/* extra for PEER: */
	struct sa remote_addr;

	uint32_t lifetime;
	bool committed;
	char *descr;

	struct mapping_table *table;  /* parent */
};

int  mapping_create(struct mapping **mappingp, struct mapping_table *table,
		    enum pcp_opcode opcode,
		    int proto,
		    const struct sa *int_addr, const struct sa *ext_addr,
		    const struct sa *remote_addr,
		    uint32_t lifetime, const uint8_t nonce[12],
		    const char *descr);
void mapping_refresh(struct mapping *mapping, uint32_t lifetime);
struct mapping *mapping_find(const struct mapping_table *table,
			     int proto, const struct sa *int_addr);
struct mapping *mapping_find_peer(const struct mapping_table *table,
				  int proto, const struct sa *int_addr,
				  const struct sa *remote_addr);


/*
 * PCP-processing API
 */

enum {
	/* minimum and maximum lifetime, in [seconds] */
	LIFETIME_MIN  =  120,
	LIFETIME_MAX  = 3600,
};

typedef bool (repcpd_pcp_msg_h)(struct udp_sock *us, const struct sa *src,
				struct mbuf *mb, struct pcp_msg *msg);

struct repcpd_pcp {
	struct le le;
	repcpd_pcp_msg_h *reqh;
};

void repcpd_register_handler(struct repcpd_pcp *pcp);
void repcpd_unregister_handler(struct repcpd_pcp *pcp);
int pcp_ereply(struct udp_sock *us, const struct sa *dst, struct mbuf *req,
	       enum pcp_result result);
bool pcp_nonce_cmp(const struct pcp_msg *msg,
		   const uint8_t nonce[PCP_NONCE_SZ]);
uint32_t pcp_lifetime_calculate(uint32_t lifetime);


/* udp */

typedef void (repcpd_udp_apply_h)(struct sa *bnd_addr, struct udp_sock *us,
				  void *arg);

void repcpd_udp_apply(repcpd_udp_apply_h *h, void *arg);


/* extaddr */

int  repcpd_extaddr_init(void);
void repcpd_extaddr_close(void);
bool repcpd_extaddr_exist(const struct sa *addr);
int  repcpd_extaddr_assign(struct sa *ext_addr, uint16_t int_port, int af);
struct sa *repcpd_extaddr_find(int af);


/*
 * Backend API
 */

typedef int  (backend_new_h)(const char *name);
typedef void (backend_flush_h)(const char *name);
typedef int  (backend_append_dnat_h)(const char *name, int proto,
				     const struct sa *ext_addr,
				     const struct sa *int_addr,
				     const char *descr);
typedef void (backend_delete_dnat_h)(const char *name, int proto,
				     const struct sa *ext_addr,
				     const struct sa *int_addr,
				     const char *descr);
typedef int  (backend_append_snat_h)(const char *name, int proto,
				     const struct sa *ext_addr,
				     const struct sa *int_addr,
				     const struct sa *peer_addr,
				     const char *descr);
typedef void (backend_delete_snat_h)(const char *name, int proto,
				     const struct sa *ext_addr,
				     const struct sa *int_addr,
				     const struct sa *peer_addr,
				     const char *descr);

struct backend {
	struct le le;
	backend_new_h *new;
	backend_flush_h *flush;
	backend_append_dnat_h *append;
	backend_delete_dnat_h *delete;
	backend_append_snat_h *append_snat;
	backend_delete_snat_h *delete_snat;
};

void backend_register(struct backend *be);
void backend_unregister(struct backend *be);
struct backend *backend_get(void);


/* div */

#ifdef STATIC
#define DECL_EXPORTS(name) exports_ ##name
#else
#define DECL_EXPORTS(name) exports
#endif

struct conf *_conf(void);
uint32_t repcpd_epoch_time(void);
int conf_get_sa(const struct conf *conf, const char *name, struct sa *sa);
