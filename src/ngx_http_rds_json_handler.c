#define DDEBUG 0
#include "ddebug.h"

#include "ngx_http_rds_json_handler.h"
#include "ngx_http_rds_json_util.h"


ngx_int_t
ngx_http_rds_json_ret_handler(ngx_http_request_t *r)
{
    ngx_chain_t                     *cl;
    ngx_buf_t                       *b;
    size_t                           len;
    ngx_http_rds_json_conf_t        *conf;
    ngx_str_t                        errstr;
    ngx_int_t                        rc;
    uintptr_t                        escape = 0;

    dd("entered ret handler");

    conf = ngx_http_get_module_loc_conf(r,
            ngx_http_rds_json_filter_module);

    /* evaluate the final value of conf->errstr */

    if (ngx_http_complex_value(r, conf->errstr, &errstr) != NGX_OK) {
        return NGX_ERROR;
    }

    /* calculate the buffer size */

    len = sizeof("{\"errcode\":") - 1
        + conf->errcode.len
        + sizeof("}") - 1
        ;

    if (errstr.len) {
        escape = ngx_http_rds_json_escape_json_str(NULL,
                errstr.data, errstr.len);

        len += sizeof(",\"errstr\":\"") - 1
             + errstr.len + escape
             + sizeof("\"") - 1
             ;
    }

    /* create the buffer */

    cl = ngx_alloc_chain_link(r->pool);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    b = ngx_create_temp_buf(r->pool, len);
    if (b == NULL) {
        return NGX_ERROR;
    }

    b->last_buf = 1;

    cl->buf = b;
    cl->next = NULL;

    /* copy data over to the buffer */

    b->last = ngx_copy_const_str(b->last, "{\"errcode\":");

    b->last = ngx_copy(b->last, conf->errcode.data, conf->errcode.len);

    if (errstr.len) {
        b->last = ngx_copy_const_str(b->last, ",\"errstr\":\"");

        if (escape == 0) {
            b->last = ngx_copy(b->last, errstr.data, errstr.len);
        } else {
            b->last = (u_char *) ngx_http_rds_json_escape_json_str(b->last,
                    errstr.data, errstr.len);
        }

        *b->last++ = '"';
    }

    *b->last++ = '}';

    if (b->last != b->end) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "rds_json: rds_json_ret: buffer error");

        return NGX_ERROR;
    }

    /* send headers */

    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_type = conf->content_type;
    r->headers_out.content_type_len = conf->content_type.len;

    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        return rc;
    }

    dd("output filter...");

    return ngx_http_output_filter(r, cl);
}

