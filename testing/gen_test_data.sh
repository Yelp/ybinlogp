#!/bin/sh
# Generate test data for ybinlogp

workdir=`mktemp -d mysqldXXXXXXXXXX -t`
pidfile="$workdir/mysqld.pid"
errfile="$workdir/mysqld.err"
datadir="$workdir/data/"
logdir="`pwd`/logs/"
tmpdir="$workdir/tmp/"
socket="$workdir/mysqld.sock"

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
    if [ -d "$workdir" ] ; then
        rm -rf "$workdir"
    fi
}

mkdir "$datadir"
mkdir -p "$logdir"
mkdir "$tmpdir"

trap 'do_at_exit' EXIT INT TERM

echo "installing db"
mysql_install_db --datadir="$datadir" --ldata="$datadir" \
    2>&1 > "$logdir/mysql_install.log"

echo "starting mysqld on $socket"
mysqld  --pid-file="$pidfile" \
    --log-error="$errfile" \
    --log-warnings \
    --datadir="$datadir" \
    --server-id=1337 \
    --skip-external-locking \
    --log_bin="$logdir/mysql-bin.log" \
    --socket="$socket" \
    --skip-networking \
    --innodb_file_per_table \
    --tmpdir="$tmpdir" \
    --innodb_buffer_pool_size=128M \
    --innodb_fast_shutdown=2 \
    --skip-innodb_checksums \
    --sync-binlog=1 \
    --max-allowed-packet=32M \
    --skip-grant-tables 2>&1 > "$logdir/mysql_run.log" &

for i in `seq 1 10` ; do
    if mysql_alive > /dev/null; then
        echo "alive"
        break
    fi
    echo -n .
    sleep 1
done
if ! mysql_alive ; then
    echo "failed to start"
    exit 1
fi

echo "writing test data."
mysql -S "$socket" < testing/gen_test_data.sql

# Start a shell to prevent exit
echo "Working dir: $workdir"
$SHELL

echo "shutting down mysql"
stop_mysql
