#define DDEBUG 1
#include "ddebug.h"

#include "resty_dbd_stream.h"
#include "ngx_http_rds_json_util.h"


ngx_int_t
ngx_http_rds_json_test_content_type(ngx_http_request_t *r)
{
    ngx_str_t           *type;

    type = &r->headers_out.content_type;
    if (type->len != rds_content_type_len ||
            ngx_strncmp(type->data, rds_content_type,
                rds_content_type_len) != 0)
    {
        return NGX_DECLINED;
    }

    return NGX_OK;
}

