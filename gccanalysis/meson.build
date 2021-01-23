project('gst-gcc-analysis', 'cpp', version : '1.17.0.1', license : 'LGPL',default_options : ['warning_level=3', 'cpp_std=c++17'])

plugins_install_dir = join_paths('/lib/x86_64-linux-gnu', 'gstreamer-1.0')

cc = meson.get_compiler('c')

gst_version = meson.project_version()

api_version = '1.0'

gst_dep = dependency('gstreamer-1.0',
    fallback : ['gstreamer', 'gst_dep'])

rtp_dep = dependency('/usr/lib/x86_64-linux-gnu/pkgconfig/gstreamer-rtp-1.0.pc',
    fallback : ['gstreamer', 'rtp_dep'])

plugin_c_args = ['-DHAVE_CONFIG_H']

cdata = configuration_data()
cdata.set_quoted('PACKAGE_VERSION', gst_version)
cdata.set_quoted('PACKAGE', 'gst-gcc-analysis')
cdata.set_quoted('GST_LICENSE', 'LGPL')
cdata.set_quoted('GST_API_VERSION', api_version)
cdata.set_quoted('GST_PACKAGE_NAME', 'GStreamer GCC Plugin')
cdata.set_quoted('GST_PACKAGE_ORIGIN', 'https://gstreamer.freedesktop.org')
configure_file(output : 'config.h', configuration : cdata)



# Plugin  (Gcc gccanalysis example)
gccanalysis_sources = [
  'src/gstgccanalysis.cpp',
  'src/log.cpp',
  ]

gstgccanalysis = library('gstgccanalysis',
  gccanalysis_sources,
  c_args: plugin_c_args,
  dependencies : [gst_dep,rtp_dep],
  install : true,
  install_dir : plugins_install_dir,
)
