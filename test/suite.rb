#!/usr/bin/env ruby
# coding: utf-8

require 'test/unit'

Dir.glob('test_*.rb').each do|f|
    test_case = File.expand_path(f, File.dirname(__FILE__)).gsub('.rb', '')
    require test_case
end
