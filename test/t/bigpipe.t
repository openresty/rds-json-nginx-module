# vi:filetype=perl

use lib 'lib';
use Test::Nginx::Socket;

#repeat_each(10);
repeat_each(1);

plan tests => repeat_each() * 2 * blocks();

no_root_location();
no_long_string();
#no_diff();

run_tests();

__DATA__

=== TEST 1: synchronous
--- http_config
    upstream pgsql {
        postgres_server 127.0.0.1:5432 dbname=test
             password=some_pass user=monty;
        postgres_keepalive off;
    }
--- config
        location / {
            echo                        "<html>(...template with javascript and divs...)";
            echo                        -n "<script type=\"text/javascript\">loader.load(";
            echo_location               /_query1;
            echo                        ")</script>";
            echo                        -n "<script type=\"text/javascript\">loader.load(";
            echo_location               /_query2;
            echo                        ")</script>";
            echo                        "</html>";
        }

        location /_query1 {
            internal;
            postgres_pass               pgsql;
            postgres_query              "SELECT * FROM cats order by id asc";
            rds_json            on;
        }

        location /_query2 {
            internal;
            postgres_pass               pgsql;
            postgres_query              "SELECT * FROM cats order by id desc";
            rds_json            on;
        }
--- request
GET /test
--- response_body
<html>(...template with javascript and divs...)
<script type="text/javascript">loader.load([{"id":2,"name":null},{"id":3,"name":"bob"}])</script>
<script type="text/javascript">loader.load([{"id":3,"name":"bob"},{"id":2,"name":null}])</script>
</html>
--- skip_nginx: 2: < 0.7.46


=== TEST 2: asynchronous (without echo filter)
--- http_config
    upstream pgsql {
        postgres_server 127.0.0.1:5432 dbname=test
             password=some_pass user=monty;
        postgres_keepalive off;
    }
--- config
        location / {
            echo                 "<html>(...template with javascript and divs...)";
            echo                 -n "<script type=\"text/javascript\">loader.load(";
            echo_location_async  /_query1;
            echo                 ")</script>";
            echo              -n "<script type=\"text/javascript\">loader.load(";
            echo_location_async  /_query2;
            echo                 ")</script>";
            echo                 "</html>";
        }

        location /_query1 {
            internal;
            postgres_pass               pgsql;
            postgres_query              "SELECT * FROM cats order by id asc";
            rds_json            on;
        }

        location /_query2 {
            internal;
            postgres_pass               pgsql;
            postgres_query              "SELECT * FROM cats order by id desc";
            rds_json            on;
        }
--- request
GET /test
--- response_body
<html>(...template with javascript and divs...)
<script type="text/javascript">loader.load([{"id":2,"name":null},{"id":3,"name":"bob"}])</script>
<script type="text/javascript">loader.load([{"id":3,"name":"bob"},{"id":2,"name":null}])</script>
</html>
--- skip_nginx: 2: < 0.7.46


=== TEST 3: asynchronous (with echo filter)
--- http_config
    upstream pgsql {
        postgres_server 127.0.0.1:5432 dbname=test
             password=some_pass user=monty;
        postgres_keepalive off;
    }
--- config
        location / {
            echo_before_body        "<html>(...template with javascript and divs...)";
            echo_before_body     -n "<script type=\"text/javascript\">loader.load(";

            # XXX we need this to help our echo filters
            echo -n " ";

            echo_location_async  /_query1;
            echo                 ")</script>";
            echo              -n "<script type=\"text/javascript\">loader.load(";
            echo_location_async  /_query2;

            echo_after_body      ")</script>";
            echo_after_body      "</html>";
        }

        location /_query1 {
            internal;
            postgres_pass               pgsql;
            postgres_query              "SELECT * FROM cats order by id asc";
            rds_json            on;
        }

        location /_query2 {
            internal;
            postgres_pass               pgsql;
            postgres_query              "SELECT * FROM cats order by id desc";
            rds_json            on;
        }
--- request
GET /test
--- response_body
<html>(...template with javascript and divs...)
<script type="text/javascript">loader.load( [{"id":2,"name":null},{"id":3,"name":"bob"}])</script>
<script type="text/javascript">loader.load([{"id":3,"name":"bob"},{"id":2,"name":null}])</script>
</html>
--- skip_nginx: 2: < 0.7.46

