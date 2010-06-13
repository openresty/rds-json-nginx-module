
/*
 * Copyright (C) agentzh
 */


#define DDEBUG 0
#include "ddebug.h"

#include "ngx_http_rds_json_filter_module.h"
#include "ngx_http_rds_json_output.h"
#include "ngx_http_rds_json_util.h"
#include "resty_dbd_stream.h"


ngx_int_t
ngx_http_rds_json_output_literal(ngx_http_request_t *r,
        ngx_http_rds_json_ctx_t *ctx,
        u_char *data, size_t len, ngx_flag_t last_buf)
{
    ngx_chain_t                 *cl;
    ngx_buf_t                   *b;

    cl = ngx_chain_get_free_buf(r->pool, &ctx->free_bufs);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    b = cl->buf;
    b->tag = ctx->tag;
    b->flush = 0;
    b->memory = 1;
    b->temporary = 0;

    b->pos = b->start = data;
    b->end = b->start + len;
    b->last = b->pos + len;

    dd("before output chain");

    if (last_buf && r == r->main) {
        b->last_buf = 1;
    }

    *ctx->last_out = cl;
    ctx->last_out = &cl->next;

    return NGX_OK;
}


ngx_int_t
ngx_http_rds_json_output_bufs(ngx_http_request_t *r,
        ngx_http_rds_json_ctx_t *ctx)
{
    ngx_int_t                rc;

    dd("entered output chain");

    dd_dump_chain_size();

    for ( ;; ) {
        if (ctx->out == NULL) {
            /* fprintf(stderr, "\n"); */
            return NGX_OK;
        }

        /* fprintf(stderr, "XXX Relooping..."); */

        rc = ngx_http_rds_json_next_body_filter(r, ctx->out);

        if (rc == NGX_ERROR || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
            return rc;
        }

        ngx_chain_update_chains(&ctx->free_bufs, &ctx->busy_bufs, &ctx->out, ctx->tag);

        ctx->last_out = &ctx->out;
    }

    /* impossible to reach here */
    return NGX_ERROR;
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
    b->flush = 0;
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
        b->last = ngx_copy_const_str(b->last, ",\"errstr\":\"");

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

    /* XXX: make this configurable */
    if (r == r->main) {
        b->last_buf = 1;
    }

    *ctx->last_out = cl;
    ctx->last_out = &cl->next;

    return NGX_OK;
}


ngx_int_t
ngx_http_rds_json_output_field(ngx_http_request_t *r,
        ngx_http_rds_json_ctx_t *ctx, u_char *data, size_t len,
        ngx_flag_t is_null)
{
    ngx_http_rds_column_t               *col;
    ngx_flag_t                           bool_val = 0;
    size_t                               size;
    ngx_chain_t                         *cl;
    ngx_buf_t                           *b;
    uintptr_t                            key_escape = 0;
    uintptr_t                            val_escape = 0;
    u_char                              *p;
    ngx_uint_t                           i;

    dd("reading row %llu, col %d, len %d",
            (unsigned long long) ctx->row,
            (int) ctx->cur_col, (int) len);

    cl = ngx_chain_get_free_buf(r->pool, &ctx->free_bufs);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    b = cl->buf;
    b->tag = ctx->tag;
    b->flush = 0;
    b->memory = 1;
    b->temporary = 1;

    /* calculate the buffer size */

    if (ctx->cur_col == 0) {
        dd("first column");
        if (ctx->row == 1) {
            dd("first column, first row");
            size = sizeof("{\"") - 1;
        } else {
            size = sizeof(",{\"") - 1;
        }
    } else {
        size = sizeof(",\"") - 1;
    }

    col = &ctx->cols[ctx->cur_col];

    key_escape = ngx_http_rds_json_escape_json_str(NULL, col->name.data,
            col->name.len);

    dd("key_escape: %d", (int) key_escape);

    size += col->name.len + key_escape
          + sizeof("\":") - 1
          ;

    if (len == 0 && ctx->field_data_rest > 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "rds_json: at least one octet should go with the field size in one buf");
        return NGX_ERROR;
    }

    if (is_null) {
        size += sizeof("null") - 1;

    } else if (len == 0) {
        dd("empty string value found");

        size += sizeof("\"\"") - 1;

    } else {
        switch (col->std_type & 0xc000) {
        case rds_rough_col_type_float:
            dd("float field found");
            /* TODO check validity of floating numbers */
            size += len;
            break;

        case rds_rough_col_type_int:
            dd("int field found");

            for (p = data, i = 0; i < len; i++, p++) {
                if (i == 0 && *p == '-') {
                    continue;
                }

                if (*p < '0' || *p > '9') {
                    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                            "rds_json: invalid integral field value: \"%*s\"",
                            len, data);
                    return NGX_ERROR;
                }
            }

            size += len;
            break;

        case rds_rough_col_type_bool:
            dd("bool field found");

            if (*data == '0' || *data == 'f' || *data == 'F'
                    || *data == '1' || *data == 't' || *data == 'T' )
            {
                if (len != 1 || ctx->field_data_rest) {
                    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                            "rds_json: invalid boolean field value leading "
                            "by \"%*s\"", len, data);
                    return NGX_ERROR;
                }

                if (*data == '0' || *data == 'f' || *data == 'F') {
                    bool_val = 0;
                    size += sizeof("false") - 1;
                } else {
                    bool_val = 1;
                    size += sizeof("true") - 1;
                }
            } else {
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                        "rds_json: output field: invalid boolean value "
                        "leading by \"%*s\"", len, data);
                return NGX_ERROR;
            }
            break;

        default:
            dd("string field found");

            /* TODO: further inspect array types and key-value types */

            val_escape = ngx_http_rds_json_escape_json_str(NULL, data, len);

            size += sizeof("\"") - 1
                  + len + val_escape
                  ;

            if (ctx->field_data_rest == 0) {
                size += sizeof("\"") - 1;
            }
        }
    }

    if (ctx->field_data_rest == 0
            && ctx->cur_col == ctx->col_count - 1)
    {
        /* last column in the row */
        size += sizeof("}") - 1;
    }

    /* allocate the buffer */

    b->start = ngx_palloc(r->pool, size);
    if (b->start == NULL) {
        return NGX_ERROR;
    }

    b->end = b->start + size;
    b->pos = b->last = b->start;

    /* fill up the buffer */

    if (ctx->cur_col == 0) {
        dd("first column");
        if (ctx->row == 1) {
            dd("first column, first row");
            *b->last++ = '{';
        } else {
            *b->last++ = ','; *b->last++ = '{';
        }
    } else {
        *b->last++ = ',';
    }

    *b->last++ = '"';

    if (key_escape == 0) {
        b->last = ngx_copy(b->last, col->name.data, col->name.len);

    } else {
        b->last = (u_char *) ngx_http_rds_json_escape_json_str(b->last,
                col->name.data, col->name.len);
    }

    *b->last++ = '"'; *b->last++ = ':';

    if (is_null) {
        dd("copy null value over");
        b->last = ngx_copy_const_str(b->last, "null");

    } else if (len == 0) {
        dd("copy emtpy string over");
        b->last = ngx_copy_const_str(b->last, "\"\"");
    } else {
        switch (col->std_type & 0xc000) {
        case rds_rough_col_type_int:
        case rds_rough_col_type_float:
            b->last = ngx_copy(b->last, data, len);

            dd("copy over int/float value: %.*s", (int) len, data);

            break;

        case rds_rough_col_type_bool:
            if (bool_val) {
                b->last = ngx_copy_const_str(b->last, "true");
            } else {
                b->last = ngx_copy_const_str(b->last, "false");
            }
            break;

        default:
            /* string */
            *b->last++ = '"';

            if (val_escape == 0) {
                b->last = ngx_copy(b->last, data, len);
            } else {
                dd("field: string value escape non-zero: %d",
                        (int) val_escape);

#if DDEBUG
                p = b->last;
#endif

                b->last = (u_char *) ngx_http_rds_json_escape_json_str(b->last,
                        data, len);

                dd("escaped value \"%.*s\" (len %d, escape %d, escape2 %d)",
                        (int) (len + val_escape),
                        p, (int) (len + val_escape),
                        (int) val_escape,
                        (int) ((b->last - p) - len));
            }

            if (ctx->field_data_rest == 0) {
                *b->last++ = '"';
            }

            break;
        }
    }

    if (ctx->field_data_rest == 0
            && ctx->cur_col == ctx->col_count - 1)
    {
        dd("last column in the row");
        *b->last++ = '}';
    }

    if (b->last != b->end) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "rds_json: output field: buffer error (%d left)",
                (int) (b->last - b->end));
        return NGX_ERROR;
    }

    *ctx->last_out = cl;
    ctx->last_out = &cl->next;

    return NGX_OK;
}


ngx_int_t
ngx_http_rds_json_output_more_field_data(ngx_http_request_t *r,
        ngx_http_rds_json_ctx_t *ctx, u_char *data, size_t len)
{
    size_t                           size = 0;
    ngx_chain_t                     *cl;
    ngx_buf_t                       *b;
    ngx_http_rds_column_t           *col;
    uintptr_t                        escape = 0;
    u_char                          *p;
    ngx_uint_t                       i;

    cl = ngx_chain_get_free_buf(r->pool, &ctx->free_bufs);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    b = cl->buf;
    b->tag = ctx->tag;
    b->flush = 0;
    b->memory = 1;
    b->temporary = 1;

    /* calculate the buffer size */

    col = &ctx->cols[ctx->cur_col];

    switch (col->std_type & 0xc000) {
    case rds_rough_col_type_int:
        for (p = data, i = 0; i < len; i++, p++) {
            if (*p < '0' || *p > '9') {
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                        "rds_json: invalid integral field value: \"%*s\"",
                        len, data);
                return NGX_ERROR;
            }
        }

        size += len;
        break;

    case rds_rough_col_type_float:
        /* TODO: check validity of floating-point field values */
        size += len;
        break;

    case rds_rough_col_type_bool:
        break;

    default:
        /* string */
        escape = ngx_http_rds_json_escape_json_str(NULL, data, len);
        size = len + escape;

        if (ctx->field_data_rest == 0) {
            size += sizeof("\"") - 1;
        }

        break;
    }

    if (ctx->field_data_rest == 0
            && ctx->cur_col == ctx->col_count - 1)
    {
        /* last column in the row */
        size += sizeof("}") - 1;
    }

    /* allocate the buffer */

    b->start = ngx_palloc(r->pool, size);
    if (b->start == NULL) {
        return NGX_ERROR;
    }
    b->end = b->start + size;
    b->pos = b->last = b->start;

    /* fill up the buffer */

    switch (col->std_type & 0xc000) {
    case rds_rough_col_type_int:
    case rds_rough_col_type_float:
        b->last = ngx_copy(b->last, data, len);
        break;

    case rds_rough_col_type_bool:
        /* no op */
        break;

    default:
        /* string */
        if (escape == 0) {
            b->last = ngx_copy(b->last, data, len);

        } else {
            dd("more field data: string value escape non-zero: %d", (int) escape);

#if DDEBUG
                p = b->last;
#endif

            b->last = (u_char *) ngx_http_rds_json_escape_json_str(b->last,
                    data, len);

            dd("escaped value \"%.*s\" (len %d, escape %d, escape2 %d)",
                    (int) (len + escape),
                    p, (int) (len + escape),
                    (int) escape,
                    (int) ((b->last - p) - len));

        }

        if (ctx->field_data_rest == 0) {
            *b->last++ = '"';
        }

        break;
    } /* switch */

    if (ctx->field_data_rest == 0 &&
            ctx->cur_col == ctx->col_count - 1)
    {
        /* last column in the row */
        *b->last++ = '}';
    }

    if (b->last != b->end) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "rds_json: output more field data: buffer error "
                "(%d left)", (int) (b->end - b->last));
        return NGX_ERROR;
    }

    *ctx->last_out = cl;
    ctx->last_out = &cl->next;

    return NGX_OK;
}

