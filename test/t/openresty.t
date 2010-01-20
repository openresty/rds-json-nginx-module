# vi:filetype=perl

use lib 'lib';
use Test::Nginx::Socket;

repeat_each(1);

plan tests => repeat_each() * 3 * blocks();

our $http_config = <<'_EOC_';
    upstream backend {
        drizzle_server 127.0.0.1:3306 dbname=test
             password=some_pass user=monty protocol=mysql;
    }
_EOC_

our $config = <<'_EOC_';
    default_type 'application/json';

    location = '/=/view/PostsByMonth/~/~' {
        if ($arg__callback !~ '^[A-Za-z]+$') {
            echo '{"errcode":400,"errstr":"Bad _callback argument"}';
        }

        echo_before_body "$arg__callback(";
        echo_after_body ");";

        if ($arg_year !~ '^\d{4}$') {
            echo_before_body "$arg__callback(";
            echo_after_body ");";

            echo '{"errcode":400,"errstr":"Bad year argument"}';
        }
        if ($arg_month !~ '^\d{1,2}$') {
            echo_before_body "$arg__callback(";
            echo_after_body ");";

            echo '{"errcode":400,"errstr":"Bad month argument"}';
        }

        drizzle_query
"select id, title, day(created) as day
from posts
where year(created) = $arg_year and month(created) = $arg_month
order by created asc";

        drizzle_pass backend;

        error_page 500 = @err500;
        error_page 502 = @err502;
        error_page 503 = @err503;
        error_page 404 = @err404;
        error_page 400 = @err400;

        rds_json on;
    }

    location @err500 {
        echo '{"errcode":500,"errstr":"Internal Server Error"}';
    }

    location @err502 {
        echo '{"errcode":502,"errstr":"Bad Gateway"}';
    }

    location @err503 {
        echo '{"errcode":503,"errstr":"Service Unavailable"}';
    }

    location @err404 {
        echo '{"errcode":404,"errstr":"Not Found"}';
    }

    location @err400 {
        echo '{"errcode":404,"errstr":"Bad Request"}';
    }

_EOC_

no_long_string();

run_tests();

#no_diff();

__DATA__

=== TEST 1: PostsByMonth view (no month arg)
--- http_config eval: $::http_config
--- config eval: $::config
--- request
GET /=/view/PostsByMonth/~/~?_callback=foo
--- response_headers
Content-Type: application/json
--- response_body
foo(
{"errcode":400,"errstr":"Bad month argument"}
);



=== TEST 2: PostsByMonth view (bad month)
--- http_config eval: $::http_config
--- config eval: $::config
--- request
GET /=/view/PostsByMonth/~/~?month=1234&_callback=foo
--- response_headers
Content-Type: application/json
--- response_body
foo(
{"errcode":400,"errstr":"Bad month argument"}
);



=== TEST 3: PostsByMonth view (emtpy result)
--- http_config eval: $::http_config
--- config eval: $::config
--- request
GET /=/view/PostsByMonth/~/~?year=1984&month=2&_callback=bar
--- response_headers
Content-Type: application/json
--- response_body
bar(
[]);



=== TEST 4: PostsByMonth view (non-emtpy result)
--- http_config eval: $::http_config
--- config eval: $::config
--- request
GET /=/view/PostsByMonth/~/~?year=2009&month=10&_callback=foo
--- response_headers
Content-Type: application/json
--- response_body
foo(
[{"id":114,"title":"Hacking on the Nginx echo module","day":15}]);


=== TEST 5: PostsByMonth view (non-emtpy result)
--- http_config eval: $::http_config
--- config eval: $::config
--- request
GET /=/view/PostsByMonth/~/~?year=2009&month=12&_callback=foo
--- response_headers
Content-Type: application/json
--- response_body
foo(
[{"id":117,"title":"Major updates to ngx_chunkin: lots of bug fixes and beginning of keep-alive support","day":4},{"id":118,"title":"ngx_memc: an extended version of ngx_memcached that supports set, add, delete, and many more commands","day":6},{"id":119,"title":"Test::Nginx::LWP and Test::Nginx::Socket are now on CPAN","day":8}]);

