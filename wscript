#! /usr/bin/env python
# encoding: utf-8

# the following two variables are used by the target "waf dist"
VERSION='0.0.1'
APPNAME='ampswitch'

# these variables are mandatory ('/' are converted automatically)
top = '.'
out = 'build'

def options(opt):
  opt.load('compiler_cxx')

def configure(conf):
  conf.load('compiler_cxx')

  conf.check(header_name='jack/jack.h')
  conf.check(header_name='boost/asio.hpp')
  conf.check(header_name='nlohmann/json.hpp')

  conf.check(lib='jack', uselib_store='jack', mandatory=True)
  conf.check(lib='pthread', uselib_store='pthread', mandatory=False)

  conf.write_config_header('config.h')

def build(bld):
  bld.program(source='src/main.cpp\
                      src/ampswitch.cpp\
                      src/kodiclient.cpp',
              use=['jack', 'pthread'],
              includes='./src',
              cxxflags='-Wall -g',
              target='ampswitch')

