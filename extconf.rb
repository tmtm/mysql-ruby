require 'mkmf'
inc, lib = dir_config('mysql', '/usr/local')
find_library('mysqlclient', 'mysql_connect', lib, "#{lib}/mysql") or exit 1
# If you have error such as 'undefined symbol', delete '#' mark follow
# lines:
#have_library('m')
#have_library('z')
have_header('mysql.h') or have_header('mysql/mysql.h') or exit 1
create_makefile("mysql")
