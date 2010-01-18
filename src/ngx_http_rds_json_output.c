
/*
 * Copyright (C) agentzh
 */


#define DDEBUG 1
#include "ddebug.h"

#include "ngx_http_rds_json_filter_module.h"
#include "ngx_http_rds_json_output.h"
#include "ngx_http_rds_json_util.h"


ngx_int_t
ngx_http_rds_json_output_literal(ngx_http_request_t *r,
        ngx_http_rds_json_ctx_t *ctx,
        u_char *data, size_t len)
{
    ngx_chain_t                 *cl;
    ngx_buf_t                   *b;

    cl = ngx_chain_get_free_buf(r->pool, &ctx->free_bufs);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    b = cl->buf;
    b->tag = ctx->tag;
    b->flush = 1;
    b->memory = 1;
    b->temporary = 0;

    b->pos = b->start = data;
    b->end = b->start + len;
    b->last = b->pos + len;

    return ngx_http_rds_json_output_chain(r, ctx, cl);
}


ngx_int_t
ngx_http_rds_json_output_chain(ngx_http_request_t *r,
        ngx_http_rds_json_ctx_t *ctx, ngx_chain_t *in)
{
    ngx_int_t               rc;

    rc = ngx_http_output_filter(r, in);

    if (rc == NGX_ERROR || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        return rc;
    }

    ngx_chain_update_chains(&ctx->free_bufs, &ctx->busy_bufs, &in, ctx->tag);

    return rc;
}


ngx_int_t
ngx_http_rds_json_output_header(ngx_http_request_t *r,
        ngx_http_rds_json_ctx_t *ctx, ngx_http_rds_header_t *header)
{
    ngx_chain_t             *cl;
    ngx_buf_t               *b;
    size_t                   size;
    uintptr_t                escape;

    cl = ngx_chain_get_free_buf(r->pool, &ctx->free_bufs);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    b = cl->buf;
    b->tag = ctx->tag;
    b->flush = 1;
    b->memory = 1;
    b->temporary = 1;

    /* calculate the buffer size */

    size = sizeof("{\"errcode\":") - 1
         + NGX_UINT16_LEN   /* errcode */
         + sizeof("}") - 1
         ;

    if (header->errstr.len) {
        escape = ngx_http_rds_json_escape_json_str(NULL, header->errstr.data,
                header->errstr.len);

        size += sizeof(",\"errstr\":\"") - 1
              + header->errstr.len + escape
              + sizeof("\"") - 1
              ;
    } else {
        escape = (uintptr_t) 0;
    }

    if (header->insert_id) {
        size += sizeof(",\"insert_id\":") - 1
              + NGX_UINT16_LEN
              ;
    }

    if (header->affected_rows) {
        size += sizeof(",\"affected_rows\":") - 1
              + NGX_UINT64_LEN
              ;
    }

    /* create the buffer */

    b->start = ngx_palloc(r->pool, size);
    if (b->start == NULL) {
        return NGX_ERROR;
    }
    b->end = b->start + size;
    b->pos = b->last = b->start;

    /* fill up the buffer */

    b->last = ngx_copy_const_str(b->last, "{\"errcode\":");

    b->last = ngx_snprintf(b->last, NGX_UINT16_LEN, "%uD",
            (uint32_t) header->std_errcode);

    if (header->errstr.len) {
        b->last = ngx_copy_const_str(b->last, "\"errstr\":");

        if (escape == 0) {
            b->last = ngx_copy(b->last, header->errstr.data,
                    header->errstr.len);
        } else {
            b->last = (u_char *) ngx_http_rds_json_escape_json_str(b->last,
                    header->errstr.data, header->errstr.len);
        }

        *b->last++ = '"';
    }

    if (header->insert_id) {
        b->last = ngx_copy_const_str(b->last, ",\"insert_id\":");
        b->last = ngx_snprintf(b->last, NGX_UINT64_LEN, "%uL",
                header->insert_id);
    }

    if (header->affected_rows) {
        b->last = ngx_copy_const_str(b->last, ",\"affected_rows\":");
        b->last = ngx_snprintf(b->last, NGX_UINT64_LEN, "%uL",
                header->affected_rows);
    }

    *b->last++ = '}';

    if (b->last > b->end) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "rds_json: output header buffer overflown");
    }

    return ngx_http_rds_json_output_chain(r, ctx, cl);
}


ngx_int_t
ngx_http_rds_json_output_field(ngx_http_request_t *r,
        ngx_http_rds_json_ctx_t *ctx, u_char *data, size_t len)
{
    ngx_http_rds_column_t               *col;

    col = &ctx->cols[ctx->cur_col];

    /* TODO */
    return NGX_OK;
}


ngx_int_t
ngx_http_rds_json_output_more_field_data(ngx_http_request_t *r,
        ngx_http_rds_json_ctx_t *ctx, u_char *data, size_t len)
{
    /* TODO */
    return NGX_OK;
}

