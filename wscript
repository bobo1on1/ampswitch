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

  conf.check(lib='jack', uselib_store='jack', mandatory=False)

  conf.write_config_header('config.h')

def build(bld):
  bld.program(source='src/main.cpp'
              use=['jack'],
              includes='./src',
              cxxflags='-Wall -g',
              target='ampswitch')

