require 'mkmf'
MYSQLDIR = "/usr/local"
$CFLAGS = "-I#{MYSQLDIR}/include"
$LDFLAGS = "-L#{MYSQLDIR}/lib/mysql"
$libs = "-lmysqlclient"
create_makefile("mysql")
