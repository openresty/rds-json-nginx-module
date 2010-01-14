
/*
 * Copyright (C) agentzh
 */


#define DDEBUG 1
#include "ddebug.h"

#include "ngx_http_rds_json_filter_module.h"
#include "ngx_http_rds_json_output.h"


ngx_int_t
ngx_http_rds_json_output_header(ngx_http_request_t *r,
        ngx_http_rds_json_ctx_t *ctx, ngx_http_rds_header_t *header)
{
    /* TODO */
    return NGX_OK;
}


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

