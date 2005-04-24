#!/usr/local/bin/ruby
# $Id: test.rb,v 1.6 2005/04/25 14:34:19 tommy Exp $

require "test/unit"
require "./mysql.o"

class TC_Mysql < Test::Unit::TestCase
  def setup()
    @host, @user, @pass, db, port, sock, flag = ARGV
    @db = db || "test"
    @port = port.to_i
    @sock = sock.nil? || sock.empty? ? nil : sock
    @flag = flag.to_i
  end
  def teardown()
  end

  def test_init()
    assert_nothing_raised{@m = Mysql.init}
    assert_nothing_raised{@m.close}
  end

  def test_real_connect()
    assert_nothing_raised{@m = Mysql.real_connect(@host, @user, @pass, @db, @port, @sock, @flag)}
    assert_nothing_raised{@m.close}
  end

  def test_connect()
    assert_nothing_raised{@m = Mysql.connect(@host, @user, @pass, @db, @port, @sock, @flag)}
    assert_nothing_raised{@m.close}
  end

  def test_new()
    assert_nothing_raised{@m = Mysql.new(@host, @user, @pass, @db, @port, @sock, @flag)}
    assert_nothing_raised{@m.close}
  end

  def test_escape_string()
    assert_equal("abc\\'def\\\"ghi\\0jkl%mno", Mysql.escape_string("abc'def\"ghi\0jkl%mno"))
  end

  def test_quote()
    assert_equal("abc\\'def\\\"ghi\\0jkl%mno", Mysql.quote("abc'def\"ghi\0jkl%mno"))
  end

  def test_get_client_info()
    assert_match(/^\d.\d+.\d+$/, Mysql.get_client_info())
  end

  def test_client_info()
    assert_match(/^\d.\d+.\d+$/, Mysql.client_info())
  end

  def test_options()
    @m = Mysql.init
    assert_equal(@m, @m.options(Mysql::INIT_COMMAND, "SET AUTOCOMMIT=0"))
    assert_equal(@m, @m.options(Mysql::OPT_COMPRESS))
    assert_equal(@m, @m.options(Mysql::OPT_CONNECT_TIMEOUT, 10))
    assert_equal(@m, @m.options(Mysql::OPT_LOCAL_INFILE, true))
#   assert_equal(@m, @m.options(Mysql::OPT_NAMED_PIPE))
#   assert_equal(@m, @m.options(Mysql::OPT_PROTOCOL, 1))
    assert_equal(@m, @m.options(Mysql::OPT_READ_TIMEOUT, 10))
    assert_equal(@m, @m.options(Mysql::OPT_WRITE_TIMEOUT, 10))
#   assert_equal(@m, @m.options(Mysql::READ_DEFAULT_FILE, "/tmp/hoge"))
    assert_equal(@m, @m.options(Mysql::READ_DEFAULT_GROUP, "test"))
    assert_equal(@m, @m.options(Mysql::SECURE_AUTH, true))
#   assert_equal(@m, @m.options(Mysql::SET_CHARSET_DIR, "??"))
    assert_equal(@m, @m.options(Mysql::SET_CHARSET_NAME, "latin1"))
#   assert_equal(@m, @m.options(Mysql::SHARED_MEMORY_BASE_NAME, "xxx"))
    assert_equal(@m, @m.connect(@host, @user, @pass, @db, @port, @sock, @flag))
    @m.close
  end

  def test_real_connect()
    @m = Mysql.init
    assert_equal(@m, @m.real_connect(@host, @user, @pass, @db, @port, @sock, @flag))
    @m.close
  end

  def test_connect()
    @m = Mysql.init
    assert_equal(@m, @m.connect(@host, @user, @pass, @db, @port, @sock, @flag))
    @m.close
  end

end

class TC_Mysql2 < Test::Unit::TestCase
  def setup()
    @host, @user, @pass, db, port, sock, flag = ARGV
    @db = db || "test"
    @port = port.to_i
    @sock = sock.nil? || sock.empty? ? nil : sock
    @flag = flag.to_i
    @m = Mysql.new(@host, @user, @pass, @db, @port, @sock, @flag)
  end
  def teardown()
    @m.close
  end

  def test_affected_rows()
    @m.query("create temporary table t (id int)")
    @m.query("insert into t values (1)")
    assert_equal(1, @m.affected_rows)
  end

  def test_autocommit()
    if @m.methods.include? "autocommit" then
      assert_equal(@m, @m.autocommit(true))
      assert_equal(@m, @m.autocommit(false))
    end
  end

#  def test_ssl_set()
#  end

  def test_more_results_next_result()
    if @m.methods.include? "more_results" then
      @m.query_with_result = false
      @m.set_server_option(Mysql::OPTION_MULTI_STATEMENTS_ON)
      @m.query("select 1,2,3; select 4,5,6")
      res = @m.store_result
      assert_equal(["1","2","3"], res.fetch_row)
      assert_equal(nil, res.fetch_row)
      assert_equal(true, @m.more_results)
      assert_equal(true, @m.more_results?)
      assert_equal(true, @m.next_result)
      res = @m.store_result
      assert_equal(["4","5","6"], res.fetch_row)
      assert_equal(nil, res.fetch_row)
      assert_equal(false, @m.more_results)
      assert_equal(false, @m.more_results?)
      assert_equal(false, @m.next_result)
    end
  end

  def test_set_server_option()
    assert_equal(@m, @m.set_server_option(Mysql::OPTION_MULTI_STATEMENTS_ON))
    assert_equal(@m, @m.set_server_option(Mysql::OPTION_MULTI_STATEMENTS_OFF))
  end

  def test_sqlstate()
    assert_equal("00000", @m.sqlstate)
    assert_raises(Mysql::Error){@m.query("hogehoge")}
    assert_equal("42000", @m.sqlstate)
  end

  def test_query_with_result()
    assert_equal(true, @m.query_with_result)
    assert_equal(false, @m.query_with_result = false)
    assert_equal(false, @m.query_with_result)
    assert_equal(true, @m.query_with_result = true)
    assert_equal(true, @m.query_with_result)
  end

  def test_reconnect()
    assert_equal(false, @m.reconnect)
    assert_equal(true, @m.reconnect = true)
    assert_equal(true, @m.reconnect)
    assert_equal(false, @m.reconnect = false)
    assert_equal(false, @m.reconnect)
  end
end

class TC_MysqlRes < Test::Unit::TestCase
  def setup()
    @host, @user, @pass, db, port, sock, flag = ARGV
    @db = db || "test"
    @port = port.to_i
    @sock = sock.nil? || sock.empty? ? nil : sock
    @flag = flag.to_i
    @m = Mysql.new(@host, @user, @pass, @db, @port, @sock, @flag)
    @m.query("create temporary table t (id int, str char(10), primary key (id))")
    @m.query("insert into t values (1, 'abc'), (2, 'defg'), (3, 'hi')")
    @res = @m.query("select * from t")
  end
  def teardown()
    @res.free
    @m.close
  end

  def test_num_fields()
    assert_equal(2, @res.num_fields)
  end

  def test_num_rows()
    assert_equal(3, @res.num_rows)
  end

  def test_fetch_row()
    assert_equal(["1","abc"], @res.fetch_row)
    assert_equal(["2","defg"], @res.fetch_row)
    assert_equal(["3","hi"], @res.fetch_row)
    assert_equal(nil, @res.fetch_row)
  end

  def test_fetch_hash()
    assert_equal({"id"=>"1", "str"=>"abc"}, @res.fetch_hash)
    assert_equal({"id"=>"2", "str"=>"defg"}, @res.fetch_hash)
    assert_equal({"id"=>"3", "str"=>"hi"}, @res.fetch_hash)
    assert_equal(nil, @res.fetch_hash)
  end

  def test_fetch_hash2()
    assert_equal({"t.id"=>"1", "t.str"=>"abc"}, @res.fetch_hash(true))
    assert_equal({"t.id"=>"2", "t.str"=>"defg"}, @res.fetch_hash(true))
    assert_equal({"t.id"=>"3", "t.str"=>"hi"}, @res.fetch_hash(true))
    assert_equal(nil, @res.fetch_hash)
  end

  def test_each()
    ary = [["1","abc"], ["2","defg"], ["3","hi"]]
    @res.each do |a|
      assert_equal(ary.shift, a)
    end
  end

  def test_each_hash()
    hash = [{"id"=>"1","str"=>"abc"}, {"id"=>"2","str"=>"defg"}, {"id"=>"3","str"=>"hi"}]
    @res.each_hash do |h|
      assert_equal(hash.shift, h)
    end
  end

  def test_data_seek()
    assert_equal(["1","abc"], @res.fetch_row)
    assert_equal(["2","defg"], @res.fetch_row)
    assert_equal(["3","hi"], @res.fetch_row)
    @res.data_seek(1)
    assert_equal(["2","defg"], @res.fetch_row)
  end
  
  def test_row_seek()
    assert_equal(["1","abc"], @res.fetch_row)
    pos = @res.row_tell
    assert_equal(["2","defg"], @res.fetch_row)
    assert_equal(["3","hi"], @res.fetch_row)
    @res.row_seek(pos)
    assert_equal(["2","defg"], @res.fetch_row)
  end

  def test_field_seek()
    assert_equal(0, @res.field_tell)
    @res.fetch_field
    assert_equal(1, @res.field_tell)
    @res.fetch_field
    assert_equal(2, @res.field_tell)
    @res.field_seek(1)
    assert_equal(1, @res.field_tell)
  end

  def test_fetch_field()
    f = @res.fetch_field
    assert_equal("id", f.name)
    assert_equal("t", f.table)
    assert_equal(nil, f.def)
    assert_equal(Mysql::Field::TYPE_LONG, f.type)
    assert_equal(11, f.length)
    assert_equal(1, f.max_length)
    assert_equal(Mysql::Field::NUM_FLAG|Mysql::Field::PRI_KEY_FLAG|Mysql::Field::PART_KEY_FLAG|Mysql::Field::NOT_NULL_FLAG, f.flags)
    assert_equal(0, f.decimals)
    f = @res.fetch_field
    assert_equal("str", f.name)
    assert_equal("t", f.table)
    assert_equal(nil, f.def)
    assert_equal(Mysql::Field::TYPE_STRING, f.type)
    assert_equal(10, f.length)
    assert_equal(4, f.max_length)
    assert_equal(0, f.flags)
    assert_equal(0, f.decimals)
    f = @res.fetch_field
    assert_equal(nil, f)
  end

  def test_fetch_fields()
    a = @res.fetch_fields
    assert_equal(2, a.size)
    assert_equal("id", a[0].name)
    assert_equal("str", a[1].name)
  end

  def test_fetch_field_direct()
    f = @res.fetch_field_direct(0)
    assert_equal("id", f.name)
    f = @res.fetch_field_direct(1)
    assert_equal("str", f.name)
    assert_raises(Mysql::Error){@res.fetch_field_direct(-1)}
    assert_raises(Mysql::Error){@res.fetch_field_direct(2)}
  end

  def test_fetch_lengths()
    assert_equal(nil,  @res.fetch_lengths())
    @res.fetch_row
    assert_equal([1, 3],  @res.fetch_lengths())
    @res.fetch_row
    assert_equal([1, 4],  @res.fetch_lengths())
    @res.fetch_row
    assert_equal([1, 2],  @res.fetch_lengths())
    @res.fetch_row
    assert_equal(nil,  @res.fetch_lengths())
  end

  def test_field_hash()
    f = @res.fetch_field
    h = {
      "name" => "id",
      "table" => "t",
      "def" => nil,
      "type" => Mysql::Field::TYPE_LONG,
      "length" => 11,
      "max_length" => 1,
      "flags" => Mysql::Field::NUM_FLAG|Mysql::Field::PRI_KEY_FLAG|Mysql::Field::PART_KEY_FLAG|Mysql::Field::NOT_NULL_FLAG,
      "decimals" => 0,
    }
    assert_equal(h, f.hash)
    f = @res.fetch_field
    h = {
      "name" => "str",
      "table" => "t",
      "def" => nil,
      "type" => Mysql::Field::TYPE_STRING,
      "length" => 10,
      "max_length" => 4,
      "flags" => 0,
      "decimals" => 0,
    }
    assert_equal(h, f.hash)
  end

  def test_field_inspect()
    f = @res.fetch_field
    assert_equal("#<Mysql::Field:id>", f.inspect)
    f = @res.fetch_field
    assert_equal("#<Mysql::Field:str>", f.inspect)
  end

  def test_is_num()
    f = @res.fetch_field
    assert_equal(true, f.is_num?)
    f = @res.fetch_field
    assert_equal(false, f.is_num?)
  end

  def test_is_not_null()
    f = @res.fetch_field
    assert_equal(true, f.is_not_null?)
    f = @res.fetch_field
    assert_equal(false, f.is_not_null?)
  end

  def test_is_pri_key()
    f = @res.fetch_field
    assert_equal(true, f.is_pri_key?)
    f = @res.fetch_field
    assert_equal(false, f.is_pri_key?)
  end

end

