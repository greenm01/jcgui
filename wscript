#! /usr/bin/env python
# encoding: utf-8

import os
import Utils
import Options
import commands
g_maxlen = 40
import shutil
import re
import Logs

# used by waf dist and waf build
VERSION='0.8'
APPNAME='Jc_Gui'

# these variables are mandatory ('/' are converted automatically)
srcdir = '.'
blddir = 'build'

# options defined for waf configure
def set_options(opt):
    opt.tool_options('compiler_cxx')

    opt.add_option('--cxxflags', 
                   type='string', 
                   default='-O3  -march=native -Wall', 
                   dest='cxxflags', 
                   help='C++ compiler flags [Default: -O3  -march=native -Wall]')

# a bit of waf display formatting
def display_msg(msg, status = None, color = None):
    sr = msg
    global g_maxlen
    g_maxlen = max(g_maxlen, len(msg))
    if status:
        print "%s :" % msg.ljust(g_maxlen),
        Utils.pprint(color, status)
    else:
        print "%s" % msg.ljust(g_maxlen)

def error_msg(msg):
    Utils.pprint('RED', msg)

def display_feature(msg, build):
    if build:
        display_msg(msg, "yes", 'GREEN')
    else:
        display_msg(msg, "no", 'YELLOW')

def check_zita_resampler(conf):

    expected_zita_resampler_version = 1
    code="""
    #include <zita-resampler/resampler.h>
    #include <zita-resampler/resampler-table.h>
    #if ZITA_RESAMPLER_MAJOR_VERSION != %d
    #error
    #endif
    int main(){ return 0; }
    """ % expected_zita_resampler_version
    conf.check_cxx(
        fragment=code,
        lib='zita-resampler',
        uselib_store='ZITA_RESAMPLER',
        msg='Checking for zita-resampler >= %d.0' % expected_zita_resampler_version,
        errmsg="version %d not found, using ours" % expected_zita_resampler_version,
        define_name="ZITA_RESAMPLER")

# guitarix waf configuration
def configure(conf):
    conf.check_tool('compiler_cxx')

    conf.check_cfg(package='jack', atleast_version='0.109.1', args='--cflags --libs', uselib_store='JACK', mandatory=1)
    conf.check_cfg(package='sndfile', atleast_version='1.0.17', args='--cflags --libs', uselib_store='SNDFILE', mandatory=1)
    conf.check_cfg(package='gdk-pixbuf-2.0', atleast_version='2.12.0', args='--cflags --libs', uselib_store='GDK', mandatory=1)
    conf.check_cfg(package='cairo', args='--cflags --libs',  mandatory=1)
    conf.check_cfg(package='pango', args='--cflags --libs',  mandatory=1)
    conf.check_cfg(package='atk', args='--cflags --libs',  mandatory=1)
    conf.check_cfg(package='glib-2.0', args='--cflags --libs',  mandatory=1)
    conf.check_cfg(package='freetype2', args='--cflags --libs',  mandatory=1)
    conf.check_cfg(package='gtk+-2.0', atleast_version='2.12.0', args='--cflags --libs', uselib_store='GTK2', mandatory=1)
    check_zita_resampler(conf)

    conf.env['SHAREDIR'] = conf.env['PREFIX'] + '/share'

    # defines for compilation
    conf.define('GX_STYLE_DIR', os.path.normpath(os.path.join(conf.env['SHAREDIR'], 'Jc_Gui')))
    conf.define('GX_PIXMAPS_DIR', os.path.normpath(os.path.join(conf.env['SHAREDIR'], 'pixmaps')))
    conf.define('GX_VERSION', VERSION)

    # writing config.h
    conf.write_config_header('config.h')

    conf.define('BINDIR', os.path.normpath(os.path.join(conf.env['PREFIX'], 'bin')))
    conf.define('DESKAPPS_DIR', os.path.normpath(os.path.join(conf.env['SHAREDIR'], 'applications')))
    conf.define('BIN_NAME', APPNAME)
    conf.define('CXXFLAGS', Options.options.cxxflags)

    # config subdirs
    conf.sub_config('src');
 
    # some output
    print
    display_msg("==================")
    version_msg = "Jc_Gui " + VERSION

    print version_msg

    print

    display_msg("C++ flags", conf.env['CXXFLAGS'], 'CYAN')
    display_msg("Install prefix", conf.env['PREFIX'], 'CYAN')
    display_msg("Install binary", conf.env['BINDIR'], 'CYAN')
    display_msg("Jc_Gui share directory", conf.env['GX_STYLE_DIR'], 'CYAN')
    display_msg("Jc_Gui pixmaps directory", conf.env['GX_PIXMAPS_DIR'], 'CYAN')

    print

def build(bld):
    # process subfolders from here
    #bld.add_subdirs('ladspa')
    bld.add_subdirs('src')
    bld.add_subdirs('pixmaps')

    bld.install_files(bld.env['DESKAPPS_DIR'], 'Jc_Gui.desktop', chmod=0644)

    
