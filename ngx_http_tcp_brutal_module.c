/*
 * Copyright (C) 2011 Nicolas Viennot <nicolas@viennot.biz>
 *
 * Copyright (C) 2023 Duoduo Song <sduoduo233@gmail.com>
 *
 * This file is subject to the terms and conditions of the MIT License.
 */

/* 包含必要的头文件 */
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>

/* 定义数据类型别名，用于brutal参数 */
typedef uint64_t u64;
typedef uint32_t u32;

/* brutal TCP拥塞控制算法的参数结构 */
struct brutal_params
{
    u64 rate;      // 发送速率，单位为字节/秒
    u32 cwnd_gain; // 拥塞窗口增益，以十分之一为单位(10=1.0)
} __packed;

extern ngx_module_t ngx_http_tcp_brutal_module;

/* http主配置结构，用于http层面的配置 */
typedef struct {
	ngx_flag_t	enable;  // 是否启用brutal TCP，可在http块中配置
} ngx_http_tcp_brutal_main_conf_t;

/* server配置结构，用于server层面的配置 */
typedef struct {
	ngx_flag_t	enable;  // 是否启用brutal TCP，可在server块中配置
} ngx_http_tcp_brutal_srv_conf_t;

/* location配置结构，用于具体location的配置 */
typedef struct {
	ngx_uint_t	rate;    // brutal TCP的传输速率设置
} ngx_http_tcp_brutal_loc_conf_t;

/* 
 * 请求处理函数
 * 当收到HTTP请求时，此函数会被调用来处理brutal TCP的设置
 */
static ngx_int_t
ngx_http_tcp_brutal_handler(ngx_http_request_t *r)
{
	ngx_http_tcp_brutal_main_conf_t *bmcf;
	ngx_http_tcp_brutal_srv_conf_t *bscf;
	ngx_http_tcp_brutal_loc_conf_t *blcf;
	int fd = r->connection->fd;

	/* 获取各个层级的配置 */
	bmcf = ngx_http_get_module_main_conf(r, ngx_http_tcp_brutal_module);
	bscf = ngx_http_get_module_srv_conf(r, ngx_http_tcp_brutal_module);
	blcf = ngx_http_get_module_loc_conf(r, ngx_http_tcp_brutal_module);

	/* 检查是否启用brutal TCP */
	if ((!bmcf->enable && !bscf->enable) || (bscf->enable == 0) || !blcf->rate) {
		// 如果http和server都未启用，或server明确配置为off，或没有设置速率，则不启用brutal
		return NGX_DECLINED;
	}

	/* 设置TCP拥塞控制算法为brutal */
	char buf[32];
	strcpy(buf, "brutal");
	socklen_t len = sizeof(buf);

	if (setsockopt(fd, IPPROTO_TCP, TCP_CONGESTION, buf, len) != 0) {
		ngx_log_error(NGX_LOG_ERR, r->connection->log, errno, "tcp brutal TCP_CONGESTION 1 error");
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}

	/* 设置brutal TCP的参数 */
	struct brutal_params params;
	params.rate = (u64)blcf->rate;  // 设置传输速率
	params.cwnd_gain = 15;          // 设置拥塞窗口增益为1.5

	// TCP_BRUTAL_PARAMS = 23301，设置brutal特定参数
	if (setsockopt(fd, IPPROTO_TCP, 23301, &params, sizeof(params)) != 0) {
		ngx_log_error(NGX_LOG_ERR, r->connection->log, errno, "tcp brutal TCP_CONGESTION 2 error");
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}

	return NGX_DECLINED;
}

/* 
 * 模块预配置
 * 在配置解析开始前被调用
 */
static ngx_int_t
ngx_http_tcp_brutal_preconfiguration(ngx_conf_t *cf)
{
    ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0, "Brutal module preconfiguration start");
    return NGX_OK;
}

/* 
 * 模块初始化函数
 * 在nginx启动时被调用，用于设置请求处理函数
 */
static ngx_int_t
ngx_http_tcp_brutal_init(ngx_conf_t *cf)
{
	ngx_http_handler_pt *h;
	ngx_http_core_main_conf_t *cmcf;
	ngx_http_tcp_brutal_main_conf_t *bmcf;
	ngx_http_core_srv_conf_t **cscfp;
	ngx_http_tcp_brutal_srv_conf_t *bscf;
	ngx_uint_t s;

	cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
	bmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_tcp_brutal_module);

	/* 打印每个server的brutal配置状态 */
	cscfp = cmcf->servers.elts;
	
	/* 添加调试日志 */
	ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0, "Brutal module initializing, found %ui servers", cmcf->servers.nelts);

	for (s = 0; s < cmcf->servers.nelts; s++) {
		bscf = ngx_http_conf_get_module_srv_conf(cscfp[s], ngx_http_tcp_brutal_module);
		ngx_http_tcp_brutal_loc_conf_t *blcf = ngx_http_conf_get_module_loc_conf(cscfp[s], ngx_http_tcp_brutal_module);
		
		if (cscfp[s]->server_name.len == 0) {
			if ((!bmcf->enable && !bscf->enable) || (bscf->enable == 0)) {
				ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0,
					"Server [default] brutal status: disabled");
			} else {
				ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0,
					"Server [default] brutal status: enabled, rate: %ui bytes/s",
					blcf->rate);
			}
		} else {
			if ((!bmcf->enable && !bscf->enable) || (bscf->enable == 0)) {
				ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0,
					"Server [%V] brutal status: disabled",
					&cscfp[s]->server_name);
			} else {
				ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0,
					"Server [%V] brutal status: enabled, rate: %ui bytes/s",
					&cscfp[s]->server_name,
					blcf->rate);
			}
		}
	}

	/* 将处理函数添加到ACCESS阶段 */
	h = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
	if (h == NULL)
		return NGX_ERROR;

	*h = ngx_http_tcp_brutal_handler;

	return NGX_OK;
}

/* 
 * 创建http主配置
 * 在解析配置文件时被调用，用于创建http层面的配置结构
 */
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

/* 
 * 初始化http主配置
 * 在配置合并阶段被调用，用于设置默认值
 */
static char *
ngx_http_tcp_brutal_init_main_conf(ngx_conf_t *cf, void *conf)
{
	ngx_http_tcp_brutal_main_conf_t *bmcf = conf;

	if (bmcf->enable == NGX_CONF_UNSET)
		bmcf->enable = 0;

	return NGX_CONF_OK;
}

/* 
 * 创建server配置
 * 在解析配置文件时被调用，用于创建server层面的配置结构
 */
static void *
ngx_http_tcp_brutal_create_srv_conf(ngx_conf_t *cf)
{
	ngx_http_tcp_brutal_srv_conf_t *conf;

	conf = ngx_palloc(cf->pool, sizeof(*conf));
	if (conf == NULL)
		return NULL;

	conf->enable = NGX_CONF_UNSET;

	return conf;
}

/* 
 * 合并server配置
 * 在配置合并阶段被调用，用于合并不同层级的server配置
 */
static char *
ngx_http_tcp_brutal_merge_srv_conf(ngx_conf_t *cf, void *parent, void *child)
{
	ngx_http_tcp_brutal_srv_conf_t *prev = parent;
	ngx_http_tcp_brutal_srv_conf_t *conf = child;

	ngx_conf_merge_value(conf->enable, prev->enable, 0);

	return NGX_CONF_OK;
}

/* 
 * 创建location配置
 * 在解析配置文件时被调用，用于创建location层面的配置结构
 */
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

/* 
 * 合并location配置
 * 在配置合并阶段被调用，用于合并不同层级的location配置
 */
static char *
ngx_http_tcp_brutal_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
	ngx_http_tcp_brutal_loc_conf_t *prev = parent;
	ngx_http_tcp_brutal_loc_conf_t *conf = child;


	/*如果当前配置设置了 rate，就使用当前配置的值
	如果当前配置没设置 rate，但上层配置设置了，就使用上层配置的值
	如果两者都没设置，就使用默认值 2*/
	ngx_conf_merge_uint_value(conf->rate, prev->rate, 2);  // 如果未设置，则默认值为2

	return NGX_CONF_OK;
}

/* 模块命令定义 */
static ngx_command_t ngx_http_tcp_brutal_commands[] = {
	{
		ngx_string("tcp_brutal"),              // 命令名称
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_FLAG,  // 可在http和server块中使用，接受on/off参数
		ngx_conf_set_flag_slot,               // 设置标志位的处理函数
		NGX_HTTP_SRV_CONF_OFFSET,            // 配置类型（server配置）
		offsetof(ngx_http_tcp_brutal_srv_conf_t, enable),  // enable字段的偏移量
		NULL
	},
	{
		ngx_string("tcp_brutal_rate"),        // 命令名称
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,  // 可在http/server/location块中使用，接受一个参数
		ngx_conf_set_num_slot,               // 设置数值的处理函数
		NGX_HTTP_LOC_CONF_OFFSET,           // 配置类型（location配置）
		offsetof(ngx_http_tcp_brutal_loc_conf_t, rate),  // rate字段的偏移量
		NULL
	},
	ngx_null_command
};

/* 模块上下文定义 */
static ngx_http_module_t  ngx_http_tcp_brutal_module_ctx = {
	ngx_http_tcp_brutal_preconfiguration,		/* preconfiguration */
	ngx_http_tcp_brutal_init,		/* postconfiguration */

	ngx_http_tcp_brutal_create_main_conf,	/* create main configuration */
	ngx_http_tcp_brutal_init_main_conf,	/* init main configuration */

	ngx_http_tcp_brutal_create_srv_conf,	/* create server configuration */
	ngx_http_tcp_brutal_merge_srv_conf,	/* merge server configuration */

	ngx_http_tcp_brutal_create_loc_conf,	/* create location configuration */
	ngx_http_tcp_brutal_merge_loc_conf 	/* merge location configuration */
};

/* 模块定义 */
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
