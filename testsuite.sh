#!/bin/sh

# compiler options
CC=cc
#OPT="-ansi -pedantic -W -Wall -O3"
OPT="-std=c89 -pipe -pedantic -O3 -Wall -W
	   -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations
	   -Wchar-subscripts -Wformat-nonliteral -Wcast-align
     -Wpointer-arith -Wbad-function-cast -Winline
     -Wcast-qual -Wshadow -Wwrite-strings
     -Wfloat-equal -Wconversion -Wno-conversion"

# speed tests
TST=yes

# thread speed tests (use POSIX and Thread Local Storage)
TLS=yes

# ----------------------------------------------------------------------

compile()
{
  $CC $OPT exception.c $1.c -o $1
}

run()
{
  if [ "$1" == "chk" ] ; then
    shift
  else
    echo -n "$1 "
  fi
  compile $1
  ./$1
}

check()
{
  RES=`run chk $1 2>&1`

  if [ "$RES" != "$2" ] ; then
    echo "$1 FAILED"
    echo "[$RES]"
    echo "differs from expected"
    echo "[$2]"
    TST=no
  else
    echo "$1 OK"
  fi
}

# ----------------------------------------------------------------------

# exception examples

check ex-01 \
"EX-lib: exiting with uncaught exception division_by_zero thrown at (ex-01.c:9)"

check ex-02 \
"finally: try failed -> division_by_zero (ex-02.c:9)
EX-lib: exiting with uncaught exception division_by_zero thrown at (ex-02.c:9)"

check ex-03 \
"catch_any: exception division_by_zero (ex-03.c:9) caught"

check ex-04 \
"catch: exception division_by_zero (ex-04.c:9) caught"

check ex-05 \
"catch: exception division_by_zero (ex-05.c:9) caught
finally: try failed -> division_by_zero (ex-05.c:9)"

check ex-06 \
"catch: exception division_by_zero (ex-06.c:9) caught
EX-lib: exiting with uncaught exception division_by_zero thrown at (ex-06.c:9)"

check ex-07 \
"catch: exception division_by_zero (ex-07.c:9) caught
finally: try failed -> division_by_zero (ex-07.c:9)
EX-lib: exiting with uncaught exception division_by_zero thrown at (ex-07.c:9)"

check ex-08 \
"catch: exception division_by_zero (ex-08.c:9) caught
finally: try failed -> division_by_zero (ex-08.c:9)
EX-lib: exiting with uncaught exception division_by_zero thrown at (ex-08.c:9)"

check ex-09 \
"catch: exception division_by_zero (ex-09.c:9) caught
finally: try failed -> division_by_zero (ex-09.c:9)
EX-lib: exiting with uncaught exception division_by_zero thrown at (ex-09.c:9)"

# protection examples

check ex-10 \
"freeing protected memory pointed by 'p'
EX-lib: exiting with uncaught exception division_by_zero thrown at (ex-10.c:11)"

check ex-11 \
"freeing protected memory pointed by 'p'
catch_any: exception division_by_zero (ex-11.c:11) caught"

check ex-12 \
"catch_any: exception division_by_zero (ex-12.c:11) caught"

check ex-13 \
"catch_any: exception division_by_zero (ex-13.c:11) caught"

check ex-14 \
"freeing protected memory pointed by 'p2'
catch_any: exception division_by_zero (ex-14.c:11) caught"

check ex-15 \
"freeing protected memory pointed by 'p'
catch: exception sig_segv (unknown:0) caught"

# threaded tests

if [ "$TLS" == yes ] ; then

SAVED_OPT="$OPT"
OPT="$OPT -lpthread"

check ex-05th \
"catch: exception division_by_zero (ex-05th.c:13) caught
finally: try failed -> division_by_zero (ex-05th.c:13)
catch: exception division_by_zero (ex-05th.c:13) caught
finally: try failed -> division_by_zero (ex-05th.c:13)
catch: exception division_by_zero (ex-05th.c:13) caught
finally: try failed -> division_by_zero (ex-05th.c:13)
catch: exception division_by_zero (ex-05th.c:13) caught
finally: try failed -> division_by_zero (ex-05th.c:13)"

check ex-14th \
"freeing protected memory pointed by 'p2'
catch_any: exception division_by_zero (ex-14th.c:14) caught
freeing protected memory pointed by 'p2'
catch_any: exception division_by_zero (ex-14th.c:14) caught
freeing protected memory pointed by 'p2'
catch_any: exception division_by_zero (ex-14th.c:14) caught
freeing protected memory pointed by 'p2'
catch_any: exception division_by_zero (ex-14th.c:14) caught"

check ex-15th \
"freeing protected memory pointed by 'p'
catch: exception sig_segv (unknown:0) caught
freeing protected memory pointed by 'p'
catch: exception sig_segv (unknown:0) caught
freeing protected memory pointed by 'p'
catch: exception sig_segv (unknown:0) caught
freeing protected memory pointed by 'p'
catch: exception sig_segv (unknown:0) caught"

OPT="$SAVED_OPT"

fi

# speed tests

if [ "$TST" == yes ] ; then

run ex-00sp
run ex-05sp
run ex-11sp

fi

# speed tests with threads

if [ "$TST" == yes -a "$TLS" == yes ] ; then

SAVED_OPT="$OPT"
OPT="$OPT -lpthread"

run ex-00ts
run ex-05ts
run ex-11ts

OPT="$SAVED_OPT"

fi
