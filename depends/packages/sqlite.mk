package=sqlite
$(package)_version=3450000
$(package)_download_path=https://www.sqlite.org/2024
$(package)_file_name=sqlite-autoconf-$($(package)_version).tar.gz
$(package)_sha256_hash=72887d57a1d8f89f52be38ef84a6353ce8c3ed55ada7864eb944abd9a495e436
$(package)_dependencies=zlib

define $(package)_set_vars
$(package)_config_opts=--disable-shared --enable-static --disable-readline --disable-dynamic-extensions
$(package)_config_opts+=--enable-threadsafe
$(package)_config_opts_linux=--with-pic
$(package)_config_opts_darwin=--with-pic
$(package)_cflags=-DSQLITE_ENABLE_FTS5
$(package)_cflags+=-DSQLITE_ENABLE_RTREE
$(package)_cflags+=-DSQLITE_ENABLE_COLUMN_METADATA
$(package)_cflags+=-DSQLITE_ENABLE_DBSTAT_VTAB
$(package)_cflags+=-DSQLITE_SECURE_DELETE
$(package)_cflags+=-DSQLITE_ENABLE_UNLOCK_NOTIFY
$(package)_cflags+=-DSQLITE_ENABLE_JSON1
$(package)_cflags+=-DSQLITE_THREADSAFE=1
$(package)_cflags+=-DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1
$(package)_cflags+=-DSQLITE_DQS=0
$(package)_cflags+=-DSQLITE_OMIT_DEPRECATED
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
