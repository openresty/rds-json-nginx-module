# vi:filetype=perl

use lib 'lib';
use Test::Nginx::Socket;

repeat_each(2);

plan tests => repeat_each() * 2 * blocks();

run_tests();

no_diff();

__DATA__

=== TEST 1: sanity
--- http_config
    upstream backend {
        drizzle_server 127.0.0.1:3306 protocol=mysql
                       dbname=ngx_test user=ngx_test password=ngx_test;
    }
--- config
    location /mysql {
        set_form_input $sql 'sql';
        set_unescape_uri $sql;
        #echo $sql;
        drizzle_query $sql;
        drizzle_pass backend;
        rds_json on;
    }
--- more_headers
Content-Type: application/x-www-form-urlencoded
--- request
POST /mysql
sql=select%20*%20from%20cats;
--- response_body chomp
[{"id":2,"name":null},{"id":3,"name":"bob"}]

