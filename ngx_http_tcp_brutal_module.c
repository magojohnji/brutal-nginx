/*
 * Copyright (C) 2011 Nicolas Viennot <nicolas@viennot.biz>
 *
 * Copyright (C) 2023 Duoduo Song <sduoduo233@gmail.com>
 *
 * This file is subject to the terms and conditions of the MIT License.
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>

typedef uint64_t u64;
typedef uint32_t u32;

struct brutal_params
{
    u64 rate;      // Send rate in bytes per second
    u32 cwnd_gain; // CWND gain in tenths (10=1.0)
} __packed;

extern ngx_module_t ngx_http_tcp_brutal_module;

typedef struct {
	ngx_flag_t	enable;
} ngx_http_tcp_brutal_main_conf_t;

typedef struct {
	ngx_uint_t	rate;
} ngx_http_tcp_brutal_loc_conf_t;

static ngx_int_t
ngx_http_tcp_brutal_handler(ngx_http_request_t *r)
{
	ngx_http_tcp_brutal_main_conf_t *bmcf;
	ngx_http_tcp_brutal_loc_conf_t *blcf;
	int fd = r->connection->fd;

	bmcf = ngx_http_get_module_main_conf(r, ngx_http_tcp_brutal_module);
	blcf = ngx_http_get_module_loc_conf(r, ngx_http_tcp_brutal_module);

	if (!bmcf->enable) {
        // 如果没有启用 brutal 流控，记录日志
        ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, "Brutal TCP flow control is not enabled for request from %V", &r->connection->addr_text);
        return NGX_DECLINED;
    }
		
    // 如果启用了 brutal 流控，记录日志
    ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, "Brutal TCP flow control is enabled for request from %V with rate %ui bytes/s", &r->connection->addr_text, blcf->rate);

	char buf[32];
	strcpy(buf, "brutal");
	socklen_t len = sizeof(buf);

	if (setsockopt(fd, IPPROTO_TCP, TCP_CONGESTION, buf, len) != 0) {
		ngx_log_error(NGX_LOG_ERR, r->connection->log, errno, "tcp brutal TCP_CONGESTION 1 error");
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}

	struct brutal_params params;
	params.rate = (u64)blcf->rate;  // 显式转换为 u64 类型
	params.cwnd_gain = 15;

	// TCP_BRUTAL_PARAMS = 23301
	if (setsockopt(fd, IPPROTO_TCP, 23301, &params, sizeof(params)) != 0) {
		ngx_log_error(NGX_LOG_ERR, r->connection->log, errno, "tcp brutal TCP_CONGESTION 2 error");
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}

	return NGX_DECLINED;
}

static ngx_int_t
ngx_http_tcp_brutal_init(ngx_conf_t *cf)
{
	ngx_http_handler_pt *h;
	ngx_http_core_main_conf_t *cmcf;

	cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

	h = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
	if (h == NULL)
		return NGX_ERROR;

	*h = ngx_http_tcp_brutal_handler;

	return NGX_OK;
}

static void *
ngx_http_tcp_brutal_create_main_conf(ngx_conf_t *cf)
{
	ngx_http_tcp_brutal_main_conf_t *conf;

	conf = ngx_palloc(cf->pool, sizeof(*conf));
	if (conf == NULL)
		return NULL;

	conf->enable = NGX_CONF_UNSET;

	return conf;
}

static char *
ngx_http_tcp_brutal_init_main_conf(ngx_conf_t *cf, void *conf)
{
	ngx_http_tcp_brutal_main_conf_t *bmcf = conf;

	if (bmcf->enable == NGX_CONF_UNSET) {
		bmcf->enable = 0;
	}

	return NGX_CONF_OK;
}

static void *
ngx_http_tcp_brutal_create_loc_conf(ngx_conf_t *cf)
{
	ngx_http_tcp_brutal_loc_conf_t *conf;

	conf = ngx_palloc(cf->pool, sizeof(*conf));
	if (conf == NULL)
		return NULL;

	conf->rate = NGX_CONF_UNSET_UINT;

	return conf;
}

static char *
ngx_http_tcp_brutal_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
	ngx_http_tcp_brutal_loc_conf_t *prev = parent;
	ngx_http_tcp_brutal_loc_conf_t *conf = child;

	ngx_conf_merge_uint_value(conf->rate, prev->rate, 2);

	return NGX_CONF_OK;
}

static ngx_command_t ngx_http_tcp_brutal_commands[] = {
	{
		ngx_string("tcp_brutal"),
		NGX_HTTP_MAIN_CONF|NGX_CONF_FLAG,
		ngx_conf_set_flag_slot,
		NGX_HTTP_MAIN_CONF_OFFSET,
		offsetof(ngx_http_tcp_brutal_main_conf_t, enable),
		NULL
	},
	{
		ngx_string("tcp_brutal_rate"),
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_num_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(ngx_http_tcp_brutal_loc_conf_t, rate),
		NULL
	},
	ngx_null_command
};

static ngx_http_module_t  ngx_http_tcp_brutal_module_ctx = {
	NULL,					/* preconfiguration */
	ngx_http_tcp_brutal_init,		/* postconfiguration */

	ngx_http_tcp_brutal_create_main_conf,	/* create main configuration */
	ngx_http_tcp_brutal_init_main_conf,	/* init main configuration */

	NULL,					/* create server configuration */
	NULL,					/* merge server configuration */

	ngx_http_tcp_brutal_create_loc_conf,	/* create location configuration */
	ngx_http_tcp_brutal_merge_loc_conf 	/* merge location configuration */
};

ngx_module_t ngx_http_tcp_brutal_module = {
	NGX_MODULE_V1,
	&ngx_http_tcp_brutal_module_ctx,	/* module context */
	ngx_http_tcp_brutal_commands,	/* module directives */ 
	NGX_HTTP_MODULE,			/* module type */
	NULL,					/* init master */
	NULL,					/* init module */
	NULL,					/* init process */
	NULL,					/* init thread */
	NULL,					/* exit thread */
	NULL,					/* exit process */
	NULL,					/* exit master */
	NGX_MODULE_V1_PADDING
};
