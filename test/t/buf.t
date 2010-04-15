# vi:filetype=perl

use lib 'lib';
use Test::Nginx::Socket;

repeat_each(1);

plan tests => repeat_each() * 2 * blocks() + 2 * repeat_each() * 3;

no_long_string();

run_tests();

#no_diff();

__DATA__

=== TEST 1: rds in a single buf
--- config
location = /single {
    default_type 'application/x-resty-dbd-stream';
    set_unescape_uri $rds $arg_rds;
    echo_duplicate 1 $rds;
    rds_json on;
}
--- request eval
my $rds = "\x{00}". # endian
"\x{03}\x{00}\x{00}\x{00}". # format version 0.0.3
"\x{00}". # result type
"\x{00}\x{00}".  # std errcode
"\x{00}\x{00}" . # driver errcode
"\x{00}\x{00}".  # driver errstr len
"".  # driver errstr data
"\x{00}\x{00}\x{00}\x{00}\x{00}\x{00}\x{00}\x{00}".  # rows affected
"\x{00}\x{00}\x{00}\x{00}\x{00}\x{00}\x{00}\x{00}".  # insert id
"\x{02}\x{00}".  # col count
"\x{01}\x{00}".  # std col type (bigint/int)
"\x{03}\x{00}".  # drizzle col type
"\x{02}\x{00}".     # col name len
"id".   # col name data
"\x{13}\x{80}".  # std col type (blob/str)
"\x{fc}\x{00}".  # drizzle col type
"\x{04}\x{00}".  # col name len
"name".  # col name data
"\x{00}";  # row list terminator

use URI::Escape;
$rds = uri_escape($rds);
"GET /single?rds=$rds"
--- response_body

