# This is script to test for mysql-ruby module.
# $Id: test.rb,v 1.3 2000/01/06 15:00:30 tommy Exp $
#
# Execute in mysql-ruby top directory.
# Modify following $host, $user, $passwd and $db if needed.
#  $host:   hostname mysql running
#  $user:   mysql username (not unix login user)
#  $passwd: mysql access passwd for $user
#  $db:     database name for this test. it must not exist before testing.

require "./mysql.o"

$host = nil	# i.e. "localhost"
$user = nil	# i.e. unix login user
$passwd = nil	# i.e. no password
$db = "rubytest"

begin
  Dir.glob("t/[0-9]*.rb").sort.each do |f|
    f =~ /^t\/\d+(.*)\.rb$/
    print $1 + "."*(20-$1.length)
    $stdout.flush
    load f
    print "ok\n"
  end
ensure
  if $created
    begin
      Mysql.new($host, $user, $passwd).drop_db $db
    rescue
    end
  end
end
