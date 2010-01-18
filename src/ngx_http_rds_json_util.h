#ifndef NGX_HTTP_RDS_JSON_UTIL_H
#define NGX_HTTP_RDS_JSON_UTIL_H

#include <ngx_core.h>
#include <ngx_http.h>


uintptr_t ngx_http_rds_json_escape_json_str(u_char *dst, u_char *src, size_t size);

ngx_int_t ngx_http_rds_json_test_content_type(ngx_http_request_t *r);

void ngx_http_rds_json_discard_bufs(ngx_pool_t *pool, ngx_chain_t *in);


#endif /* NGX_HTTP_RDS_JSON_UTIL_H */

