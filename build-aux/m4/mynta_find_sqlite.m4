dnl Copyright (c) 2024-2026 The Mynta Core developers
dnl Distributed under the MIT software license, see the accompanying
dnl file COPYING or http://www.opensource.org/licenses/mit-license.php.

AC_DEFUN([MYNTA_FIND_SQLITE],[
  AC_ARG_VAR(SQLITE_CFLAGS, [C compiler flags for SQLite, bypasses autodetection])
  AC_ARG_VAR(SQLITE_LIBS, [Linker flags for SQLite, bypasses autodetection])

  if test "x$SQLITE_CFLAGS" = "x"; then
    AC_MSG_CHECKING([for SQLite headers])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
      #include <sqlite3.h>
    ]],[[
      #if SQLITE_VERSION_NUMBER < 3030000
        #error "SQLite version too old, need at least 3.30.0"
      #endif
    ]])],[
      AC_MSG_RESULT(yes)
    ],[
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([sqlite3.h headers missing or too old. ]AC_PACKAGE_NAME[ requires SQLite >= 3.30.0 for wallet functionality (--disable-wallet to disable wallet functionality)])
    ])
  else
    SQLITE_CPPFLAGS="${SQLITE_CFLAGS}"
  fi
  AC_SUBST(SQLITE_CPPFLAGS)

  if test "x$SQLITE_LIBS" = "x"; then
    AC_CHECK_LIB([sqlite3],[sqlite3_open_v2],[
      SQLITE_LIBS="-lsqlite3 -lpthread"
    ],[
      AC_MSG_ERROR([libsqlite3 missing. ]AC_PACKAGE_NAME[ requires SQLite for wallet functionality (--disable-wallet to disable wallet functionality)])
    ],[-lpthread])
  fi
  AC_SUBST(SQLITE_LIBS)

  dnl Verify SQLite was compiled with required features
  AC_MSG_CHECKING([for SQLite with required features])
  TEMP_LIBS="$LIBS"
  LIBS="$SQLITE_LIBS $LIBS"
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[
    #include <sqlite3.h>
  ]],[[
    sqlite3 *db;
    sqlite3_open_v2(":memory:", &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 0);
    sqlite3_wal_checkpoint_v2(db, 0, SQLITE_CHECKPOINT_FULL, 0, 0);
    sqlite3_close(db);
  ]])],[
    AC_MSG_RESULT(yes)
  ],[
    AC_MSG_RESULT(no)
    AC_MSG_ERROR([SQLite library missing required WAL support])
  ])
  LIBS="$TEMP_LIBS"
])
