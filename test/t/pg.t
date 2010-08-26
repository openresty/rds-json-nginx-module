# vi:filetype=perl

use lib 'lib';
use Test::Nginx::Socket;

repeat_each(1);

plan tests => repeat_each() * 2 * blocks();

our $http_config = <<'_EOC_';
    upstream database {
        postgres_server  127.0.0.1:5432 dbname=ngx_test
                         user=ngx_test password=ngx_test;
    }
_EOC_

no_long_string();

run_tests();

__DATA__

=== TEST 1: bool blob field (keepalive off)
--- http_config eval: $::http_config
--- config
    location /test {
        echo_location /pg "drop table if exists foo";
        echo;
        echo_location /pg "create table foo (id serial, flag bool);";
        echo;
        echo_location /pg "insert into foo (flag) values (true);";
        echo;
        echo_location /pg "insert into foo (flag) values (false);";
        echo;
        echo_location /pg "select * from foo order by id;";
        echo;
    }
    location /pg {
        postgres_pass backend;
        postgres_query $query_string;
        rds_json on;
    }
--- request
GET /test
--- response_body
{"errcode":0}
{"errcode":0}
{"errcode":0,"affected_rows":1}
{"errcode":0,"affected_rows":1}
[{"id":1,"flag":true},{"id":2,"flag":false}]
--- skip_nginx: 2: < 0.7.46



=== TEST 2: bool blob field (keepalive on)
--- http_config eval: $::http_config
--- config
    location /test {
        echo_location /pg "drop table if exists foo";
        echo;
        echo_location /pg "create table foo (id serial, flag bool);";
        echo;
        echo_location /pg "insert into foo (flag) values (true);";
        echo;
        echo_location /pg "insert into foo (flag) values (false);";
        echo;
        echo_location /pg "select * from foo order by id;";
        echo;
    }
    location /pg {
        postgres_pass backend;
        postgres_query $query_string;
        rds_json on;
    }
--- request
GET /test
--- response_body
{"errcode":0}
{"errcode":0}
{"errcode":0,"affected_rows":1}
{"errcode":0,"affected_rows":1}
[{"id":1,"flag":true},{"id":2,"flag":false}]
--- skip_nginx: 2: < 0.7.46

