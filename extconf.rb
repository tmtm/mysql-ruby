require 'mkmf'
MYSQLDIR = "/usr/local"

incdir = with_config("mysql-include-dir")
if incdir then
  $CFLAGS += "-I#{incdir}"
else
  $CFLAGS = "-I#{MYSQLDIR}/include"
end

libdir = with_config("mysql-lib-dir")
if libdir then
  $LDFLAGS += "-L#{libdir}"
else
  $LDFLAGS = "-L#{MYSQLDIR}/lib/mysql"
end

$libs = "-lmysqlclient"
create_makefile("mysql")
