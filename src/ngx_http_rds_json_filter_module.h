
/*
 * Copyright (C) agentzh
 */

#ifndef NGX_HTTP_RDS_JSON_FILTER_MODULE_H
#define NGX_HTTP_RDS_JSON_FILTER_MODULE_H

#include <ngx_core.h>
#include <ngx_http.h>


typedef enum {
    json_format_none,
    json_format_compact,
    json_format_pretty          /* TODO */

} ngx_http_rds_json_format_t;


typedef struct {
    ngx_http_rds_json_format_t      format;
    ngx_str_t                       content_type;

} ngx_http_rds_json_conf_t;


typedef enum {
    state_expect_header,
    state_expect_col,
    state_expect_row,
    state_expect_field,
    state_expect_more_field_data

} ngx_http_rds_json_state_t;


typedef struct {
    ngx_http_rds_json_state_t            state;

    ngx_str_t                           *col_name;
    ngx_uint_t                           col_count;
    ngx_uint_t                           cur_col;

    uint32_t                             field_offset;
    uint32_t                             field_total;

} ngx_http_rds_json_ctx_t;


#endif /* NGX_HTTP_RDS_JSON_FILTER_MODULE_H */

