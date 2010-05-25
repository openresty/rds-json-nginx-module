# vi:filetype=perl

use lib 'lib';
use Test::Nginx::Socket;

#repeat_each(10);
repeat_each(1);

plan tests => repeat_each() * 2 * blocks();

no_long_string();

run_tests();

__DATA__

=== TEST 1: bool blob field
--- http_config
    upstream backend {
        postgres_server 127.0.0.1:5432 dbname=test
             password=some_pass user=monty;
    }
--- config
    location /test {
        echo_location /mysql "drop table if exists foo";
        echo;
        echo_location /mysql "create table foo (id serial, flag bool);";
        echo;
        echo_location /mysql "insert into foo (flag) values (true);";
        echo;
        echo_location /mysql "insert into foo (flag) values (false);";
        echo;
        echo_location /mysql "select * from foo order by id;";
        echo;
    }
    location /mysql {
        postgres_pass backend;
        postgres_query $query_string;
        rds_json on;
    }
--- request
GET /test
--- response_body
{"errcode":1}
{"errcode":1}
{"errcode":1,"affected_rows":1}
{"errcode":1,"affected_rows":1}
[{"id":1,"flag":true},{"id":2,"flag":false}]
--- skip_nginx: 2: < 0.7.46

