
lib = File.expand_path("../lib", __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)

Gem::Specification.new do |spec|
  spec.name          = "mysql"
  spec.version       = '2.9'
  spec.licenses      = ['Ruby']
  spec.authors       = ["TOMITA Masahiro"]
  spec.email         = ["tommy@tmtm.org"]

  spec.summary       = 'MySQL/Ruby'
  spec.description   = 'MySQL API module for Ruby'
  spec.homepage      = 'https://github.com/tmtm/mysql-ruby'
  spec.required_ruby_version = '>= 2.2.0'

  spec.files         = `git ls-files README.md ext`.split
  spec.extensions    = 'ext/mysql/extconf.rb'
end
