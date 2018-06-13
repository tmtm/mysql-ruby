# MySQL/Ruby

This is the <a href="http://www.mysql.com">MySQL</a> API module for Ruby.
It provides the same functions for Ruby programs that the MySQL C API provides for C programs.

## Requirement

* MySQL >= 5.5 && < 8.0
* Ruby >= 2.2

## Installation

```
% ruby extconf.rb --with-mysql-dir=/usr/local/mysql
```

Or

```
% ruby extconf.rb --with-mysql-config
```

extconf.rb options:

| option                                        | description
|-----------------------------------------------|-------------------
| `--with-mysql-include=dir`                    | MySQL header file directory (default: `/usr/local/include`)
| `--with-mysql-lib=dir`                        | MySQL library directory (default: `/usr/local/lib`)
| `--with-mysql-dir=dir`                        | Same as `--with-mysql-include=dir/include --with-mysql-lib=dir/lib`
| `--with-mysql-config[=/path/to/mysql_config]` | Get compile-parameter from `mysql_config` command

And then execute:

```
% make
```

test:

```
% ruby ./test.rb -- hostname user passwd dbname port socket flag
```

If you get error like 'libmysqlclient not found' when testing, you need to specify the directory in which the library is located so that make can find it.

```
% env LD_RUN_PATH=[libmysqlclient.so directory] make
```

install:

```
% make install
```

## Usage

```ruby
require 'mysql'

my = Mysql.new(host, user, passwd, database, port)
my.query('insert into ...')
my.query('select ...') do |res|
  ...
end
```

## Author

TOMITA Masahiro [@tmtms](https://twitter.com/tmtms)
