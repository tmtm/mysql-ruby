require 'mkmf'
inc, lib = dir_config('mysql', '/usr/local')
libs = ['m','z','socket','nsl']
while not find_library('mysqlclient', 'mysql_query', lib, "#{lib}/mysql") do
  exit 1 if libs.empty?
  have_library(libs.shift)
end
have_header('mysql.h') or have_header('mysql/mysql.h') or exit 1
create_makefile("mysql")
