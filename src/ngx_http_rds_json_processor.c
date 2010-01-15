/*
 * Copyright (C) agentzh
 */


#define DDEBUG 1
#include "ddebug.h"

#include "ngx_http_rds_json_processor.h"
#include "ngx_http_rds_json_util.h"
#include "ngx_http_rds_json_output.h"
#include "ngx_http_rds.h"
#include "ngx_http_rds_utils.h"

#include <ngx_core.h>
#include <ngx_http.h>

ngx_int_t
ngx_http_rds_json_process_header(ngx_http_request_t *r,
        ngx_chain_t *in, ngx_http_rds_json_ctx_t *ctx)
{
    ngx_buf_t                       *b;
    ngx_http_rds_header_t            header;
    ngx_int_t                        rc;

    if (in == NULL) {
        return NGX_OK;
    }

    b = in->buf;

    if ( ! ngx_buf_in_memory(b)) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "rds_json: buf from upstream not in memory");
        return NGX_ERROR;
    }

    rc = ngx_http_rds_parse_header(r, b, &header);

    if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    if (header.col_count == 0) {
        /* for empty result set, just return the JSON
         * representation of the RDS header */

        if (b->pos != b->last) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "rds_json: there's unexpected remaining data in the buf");
            return NGX_ERROR;
        }

        ctx->state = state_done;

        rc = ngx_http_rds_json_output_header(r, ctx, &header);

        if (rc == NGX_ERROR || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
            return rc;
        }

        ngx_http_rds_json_discard_bufs(r->pool, in);

        return rc;
    }

    ctx->cols = ngx_palloc(r->pool,
            header.col_count * sizeof(ngx_http_rds_column_t));

    if (ctx->cols == NULL) {
        return NGX_ERROR;
    }

    ctx->state = state_expect_col;
    ctx->cur_col = 0;
    ctx->col_count = header.col_count;

    return ngx_http_rds_json_process_col(r,
            b->pos == b->last ? in->next : in, ctx);
}


ngx_int_t
ngx_http_rds_json_process_col(ngx_http_request_t *r,
        ngx_chain_t *in, ngx_http_rds_json_ctx_t *ctx)
{
    ngx_buf_t                       *b;
    ngx_int_t                        rc;

    if (in == NULL) {
        return NGX_OK;
    }

    b = in->buf;

    if ( ! ngx_buf_in_memory(b)) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "rds_json: buf from upstream not in memory");
        return NGX_ERROR;
    }

    rc = ngx_http_rds_parse_col(r, b, &ctx->cols[ctx->cur_col]);

    if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    ctx->cur_col++;

    if (ctx->cur_col >= ctx->col_count) {
        /* end of column list */

        ctx->state = state_expect_row;
        ctx->row = 0;

        /* XXX output "[" */
        rc = ngx_http_rds_json_output_literal(r, ctx,
                (u_char *)"[", sizeof("[") - 1);

        if (rc == NGX_ERROR || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
            return rc;
        }

        return ngx_http_rds_json_process_row(r,
                b->pos == b->last ? in->next : in, ctx);
    }

    return ngx_http_rds_json_process_col(r,
            b->pos == b->last ? in->next : in, ctx);
}


ngx_int_t
ngx_http_rds_json_process_row(ngx_http_request_t *r,
        ngx_chain_t *in, ngx_http_rds_json_ctx_t *ctx)
{
    ngx_buf_t                   *b;
    ngx_int_t                    rc;

    if (in == NULL) {
        return NGX_OK;
    }

    b = in->buf;

    if ( ! ngx_buf_in_memory(b)) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "rds_json: buf from upstream not in memory");
        return NGX_ERROR;
    }

    if (b->last - b->pos < (ssize_t) sizeof(uint8_t)) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
               "rds_json: row flag is incomplete in the buf");
        return NGX_ERROR;
    }

    if (*b->pos++ == 0) {
        /* end of row list */
        ctx->state = state_done;

        if (b->last != b->pos) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "rds_json: there's unexpected remaining data in the buf");
            return NGX_ERROR;
        }

        rc = ngx_http_rds_json_output_literal(r, ctx,
                (u_char *)"]", sizeof("]") - 1);

        if (rc == NGX_ERROR || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
            return rc;
        }

        return rc;
    }

    ctx->row++;
    ctx->cur_col = 0;
    ctx->state = state_expect_field;

    return ngx_http_rds_json_process_field(r,
            b->pos == b->last ? in->next : in, ctx);
}


ngx_int_t
ngx_http_rds_json_process_field(ngx_http_request_t *r,
        ngx_chain_t *in, ngx_http_rds_json_ctx_t *ctx)
{
    size_t              total, len;
    ngx_buf_t          *b;
    ngx_int_t           rc;

    for (;;) {
        if (in == NULL) {
            return NGX_OK;
        }

        b = in->buf;

        if ( ! ngx_buf_in_memory(b)) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "rds_json: buf from upstream not in memory");
            return NGX_ERROR;
        }

        if (b->last - b->pos < (ssize_t) sizeof(uint32_t)) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                   "rds_json: field size is incomplete in the buf");
            return NGX_ERROR;
        }

        total = *(uint32_t *) b->last;

        len = (uint32_t) (b->last - b->pos);

        if (len >= total) {
            len = total;
        }

        ctx->field_data_rest = total - len;

        rc = ngx_http_rds_json_output_field(r, ctx, b->pos, len);

        if (rc == NGX_ERROR || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
            return rc;
        }

        b->pos += len;

        if (b->pos == b->last) {

            if (b->temporary) {
                ngx_pfree(r->pool, b->start);
            }

            in = in->next;
        }

        if (len < total) {
            /* need to read more field data */

            ctx->state = state_expect_more_field_data;

            return ngx_http_rds_json_process_more_field_data(r, in, ctx);
        }

        ctx->cur_col++;

        if (ctx->cur_col >= ctx->col_count) {
            /* reached the end of the current row */

            ctx->state = state_expect_row;

            return ngx_http_rds_json_process_row(r, in, ctx);
        }

        /* continue to process the next field (if any) */
    }

    /* impossible to reach here */

    return NGX_ERROR;
}


ngx_int_t
ngx_http_rds_json_process_more_field_data(ngx_http_request_t *r,
        ngx_chain_t *in, ngx_http_rds_json_ctx_t *ctx)
{
    ngx_int_t                    rc;
    ngx_buf_t                   *b;
    size_t                       len;

    for (;;) {
        if (in == NULL) {
            return NGX_OK;
        }

        b = in->buf;

        if ( ! ngx_buf_in_memory(b)) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "rds_json: buf from upstream not in memory");
            return NGX_ERROR;
        }

        len = b->last - b->pos;

        if (len >= ctx->field_data_rest) {
            len = ctx->field_data_rest;
            ctx->field_data_rest = 0;

        } else {
            ctx->field_data_rest -= len;
        }

        rc = ngx_http_rds_json_output_more_field_data(r, ctx, b->pos, len);

        if (rc == NGX_ERROR || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
            return rc;
        }

        if (b->pos == b->last) {
            if (b->temporary) {
                ngx_pfree(r->pool, b->start);
            }
            in = in->next;
        }

        if (len < ctx->field_data_rest) {
            /* still some data remaining */
            continue;
        }

        /* reached the end of the current field */

        ctx->cur_col++;

        if (ctx->cur_col >= ctx->col_count) {
            /* reached the end of the current row */

            ctx->state = state_expect_row;

            return ngx_http_rds_json_process_row(r, in, ctx);
        }

        /* read the next field */

        ctx->state = state_expect_field;

        return ngx_http_rds_json_process_field(r, in, ctx);
    }

    /* impossible to reach here */

    return NGX_ERROR;
}

