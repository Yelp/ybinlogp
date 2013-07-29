#!/bin/sh

tdir=`mktemp -d mysqldXXXXXXXXXX -t`
pidfile="$tdir/mysqld.pid"
errfile="$tdir/mysqld.err"
datadir="$tdir/data/"
logdir="`pwd`"
tmpdir="$tdir/tmp/"
socket="$tdir/mysqld.sock"

stop_mysql() {
    pid=`cat "$pidfile" 2>/dev/null`
    if [ -z "$pid" ] ; then
        return
    fi
    if kill -0 "$pid" ; then
        mysqladmin -S "$socket" shutdown 2>&1 > /dev/null
    fi
}

mysql_alive() {
    mysqladmin -S "$socket" ping 2>&1 > /dev/null
    return $?
}

do_at_exit() {
    if [ -d "$tdir" ] ; then
        rm -rf "$tdir"
    fi
}

mkdir "$datadir"
mkdir "$logdir"
mkdir "$tmpdir"

trap 'do_at_exit' EXIT INT TERM

echo "installing db"
mysql_install_db --datadir="$datadir" --ldata="$datadir" 2>&1 > /dev/null
echo "starting mysqld on $socket"
mysqld  --pid-file="$pidfile" --log-error="$errfile" --log-warnings --datadir="$datadir" --server-id=1337 --skip-external-locking --log_bin="$logdir/mysql-bin.log" --socket="$socket" --skip-networking --innodb_file_per_table --tmpdir="$tmpdir" --innodb_buffer_pool_size=128M --innodb_fast_shutdown=2 --skip-innodb_checksums --sync-binlog=1 --max-allowed-packet=32M --skip-grant-tables 2>&1 > /dev/null&
for i in `seq 1 10` ; do
    if mysql_alive ; then
        echo "alive"
        break
    fi
    printf "."
    sleep 1
done
if ! mysql_alive ; then
    echo "failed to start"
    exit 1
fi

mysql -S "$socket" < gen_test_data.sql

#cp -r $logdir/* .
#rm mysql-bin.index
#ls mysql-bin*

zsh

stop_mysql
