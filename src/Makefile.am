# Note: plugindir is set in configure

plugin_LTLIBRARIES = libgstacceltransform.la

## Plugin 1

# sources used to compile this plug-in
libgstacceltransform_la_SOURCES = gstacceltransform.c cmempool.c cmem_buf.c v4l2_m2m.c

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstacceltransform_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS)
libgstacceltransform_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) -lgstvideo-1.0 $(GST_BASE_LIBS) $(GST_LIBS) -lticmem
libgstacceltransform_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstacceltransform_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstacceltransform.h cmempool.h cmem_buf.h v4l2_m2m.h
