require 'mkmf'
$CFLAGS = "-g -I/usr/local/include"
$LDFLAGS = "-L/usr/local/lib -L/usr/local/lib/mysql"
$libs = "-lmysqlclient -lsocket -lnsl"
create_makefile("mysql")
