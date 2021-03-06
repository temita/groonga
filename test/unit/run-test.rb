#!/usr/bin/env ruby

$KCODE = 'utf-8' unless defined?(Encoding)

require 'rbconfig'
require 'fileutils'
require "rubygems"
gem "test-unit", ">= 2.3.1"
require "test/unit"
require "test/unit/notify"
require "json"

build_dir = File.expand_path(ENV["BUILD_DIR"] || File.dirname(__FILE__))
base_dir = File.expand_path(ENV["BASE_DIR"] || File.dirname(__FILE__))

test_lib_dir = File.expand_path(File.join(build_dir, "..", "lib"))
FileUtils.mkdir_p(test_lib_dir)

$LOAD_PATH.unshift(File.join(base_dir, "lib", "ruby"))
$LOAD_PATH.unshift(File.expand_path(File.join(base_dir, "..", "..", "tools")))

require 'groonga-test-utils'
require 'groonga-http-test-utils'
require 'groonga-local-gqtp-test-utils'
require 'groonga-grntest-test-utils'

ARGV.unshift("--exclude", "run-test.rb")
ARGV.unshift("--show-detail-immediately")
exit Test::Unit::AutoRunner.run(true, File.dirname($0))
