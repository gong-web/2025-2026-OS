#!/usr/bin/env bash

verbose=false
if [ "x$1" = "x-v" ]; then
    verbose=true
    out=/dev/stdout
    err=/dev/stderr
else
    out=/dev/null
    err=/dev/null
fi

# Ensure the RISC-V toolchain is available when running under WSL.
# Prefer an already-activated environment; otherwise try common activation scripts.
if ! command -v riscv64-unknown-elf-gcc > /dev/null 2>&1; then
    script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
    repo_root="$(cd -- "${script_dir}/../../.." && pwd)"

    candidates=(
        "/mnt/d/gds/Documents/Operating_system/riscv_isolated/scripts/activate_riscv_env.sh"
        "${repo_root}/labcode/env/activate_os_env.sh"
    )

    for f in "${candidates[@]}"; do
        if [ -f "$f" ]; then
            # shellcheck disable=SC1090
            source "$f" > /dev/null 2>&1 || true
            if command -v riscv64-unknown-elf-gcc > /dev/null 2>&1; then
                break
            fi
        fi
    done

    if ! command -v riscv64-unknown-elf-gcc > /dev/null 2>&1; then
        echo "riscv64-unknown-elf-gcc: not found (toolchain env not activated)" >&2
        echo "Hint: run 'bash run_grade_wsl.sh' once, or source your RISC-V env script, then rerun." >&2
        exit 1
    fi
fi

## make & makeopts
if gmake --version > /dev/null 2>&1; then
    make=gmake;
else
    make=make;
fi

makeopts="--quiet --no-print-directory -j"

make_print() {
    echo `$make $makeopts print-$1`
}
echo `$make`

## command tools
awk='awk'
bc='bc'
date='date'
grep='grep'
rm='rm -f'
sed='sed'

## symbol table
sym_table='obj/kernel.sym'

## gdb & gdbopts
gdb="$(make_print GDB)"
gdbport='1234'

gdb_timeout='10'

gdb_in="$(make_print GRADE_GDB_IN)"

## qemu & qemuopts
qemu="$(make_print qemu)"

qemu_out="$(make_print GRADE_QEMU_OUT)"

if $qemu -nographic -help | grep -q '^-gdb'; then
    qemugdb="-gdb tcp::$gdbport"
else
    qemugdb="-s -p $gdbport"
fi

## default variables
default_timeout=90
default_pts=5

qemu_run_secs=${QEMU_RUN_SECS:-15}

pts=5
part=0
part_pos=0
total=0
total_pos=0

## default functions
update_score() {
    total=`expr $total + $part`
    total_pos=`expr $total_pos + $part_pos`
    part=0
    part_pos=0
}

get_time() {
    echo `$date +%s.%N 2> /dev/null`
}

show_part() {
    echo "Part $1 Score: $part/$part_pos"
    echo
    update_score
}

show_final() {
    update_score
    echo "Total Score: $total/$total_pos"
    if [ $total -lt $total_pos ]; then
        exit 1
    fi
}

show_time() {
    t1=$(get_time)
    time=`echo "scale=1; ($t1-$t0)/1" | $sed 's/.N/.0/g' | $bc 2> /dev/null`
    echo "(${time}s)"
}

show_build_tag() {
    echo "$1:" | $awk '{printf "%-24s ", $0}'
}

show_check_tag() {
    echo "$1:" | $awk '{printf "  -%-40s  ", $0}'
}

show_msg() {
    echo $1
    shift
    if [ $# -gt 0 ]; then
        echo -e "$@" | awk '{printf "   %s\n", $0}'
        echo
    fi
}

pass() {
    show_msg OK "$@"
    part=`expr $part + $pts`
    part_pos=`expr $part_pos + $pts`
}

fail() {
    show_msg WRONG "$@"
    part_pos=`expr $part_pos + $pts`
}

run_qemu() {
    qemuextra=
    if [ "$brkfun" ]; then
        qemuextra="-S $qemugdb"
    fi

    if [ -z "$timeout" ] || [ $timeout -le 0 ]; then
        timeout=$default_timeout;
    fi

    t0=$(get_time)
    (
        ulimit -t $timeout
        exec $qemu -nographic $qemuopts -serial file:$qemu_out -monitor null -no-reboot $qemuextra
    ) > $out 2> $err &
    pid=$!

    sleep 1

    if [ -n "$brkfun" ]; then
        brkaddr=`$grep " $brkfun\$" $sym_table | $sed -e's/ .*$//g'`
        brkaddr_phys=`echo $brkaddr | sed "s/^c0/00/g"`
        (
            echo "set confirm off"
            echo "set pagination off"
            echo "set remotetimeout 2"
            echo "target remote localhost:$gdbport"
            echo "break *0x$brkaddr"
            if [ "$brkaddr" != "$brkaddr_phys" ]; then
                echo "break *0x$brkaddr_phys"
            fi
            echo "continue"
        ) > $gdb_in

        if command -v timeout > /dev/null 2>&1; then
            timeout ${GDB_TIMEOUT:-$gdb_timeout}s $gdb -batch -nx -x $gdb_in > /dev/null 2>&1
        else
            $gdb -batch -nx -x $gdb_in > /dev/null 2>&1
        fi
        kill $pid > /dev/null 2>&1
    else
        # 无 GDB 模式：让内核/用户程序跑一段时间，收集串口输出后再结束 QEMU。
        sleep $qemu_run_secs
        kill $pid > /dev/null 2>&1
    fi
}

build_run() {
    show_build_tag "$1"
    shift

    if $verbose; then
        echo "$make $@ ..."
    fi
    $make $makeopts $@ 'DEFS+=-DDEBUG_GRADE' > $out 2> $err

    if [ $? -ne 0 ]; then
        echo $make $@ failed
        exit 1
    fi

    run_qemu
    show_time
    cp $qemu_out .`echo $tag | tr '[:upper:]' '[:lower:]' | sed 's/ /_/g'`.log
}

check_result() {
    show_check_tag "$1"
    shift
    if [ ! -s $qemu_out ]; then
        sleep 4
    fi
    if [ ! -s $qemu_out ]; then
        fail > /dev/null
        echo 'no $qemu_out'
    else
        check=$1
        shift
        $check "$@"
    fi
}

check_regexps() {
    okay=yes
    not=0
    reg=0
    error=
    for i do
        if [ "x$i" = "x!" ]; then
            not=1
        elif [ "x$i" = "x-" ]; then
            reg=1
        else
            if [ $reg -ne 0 ]; then
                $grep '-E' "^$i\$" $qemu_out > /dev/null
            else
                $grep '-F' "$i" $qemu_out > /dev/null
            fi
            found=$(($? == 0))
            if [ $found -eq $not ]; then
                if [ $found -eq 0 ]; then
                    msg="!! error: missing '$i'"
                else
                    msg="!! error: got unexpected line '$i'"
                fi
                okay=no
                if [ -z "$error" ]; then
                    error="$msg"
                else
                    error="$error\n$msg"
                fi
            fi
            not=0
            reg=0
        fi
    done
    if [ "$okay" = "yes" ]; then
        pass
    else
        fail "$error"
        if $verbose; then
            exit 1
        fi
    fi
}

run_test() {
    tag=
    prog=
    check=check_regexps
    while true; do
        select=
        case $1 in
            -tag|-prog)
                select=`expr substr $1 2 ${#1}`
                eval $select='$2'
                ;;
        esac
        if [ -z "$select" ]; then
            break
        fi
        shift
        shift
    done
    defs=
    while expr "x$1" : "x-D.*" > /dev/null; do
        defs="DEFS+='$1' $defs"
        shift
    done
    if [ "x$1" = "x-check" ]; then
        check=$2
        shift
        shift
    fi

    if [ -z "$prog" ]; then
        $make $makeopts touch > /dev/null 2>&1
        args="$defs"
    else
        if [ -z "$tag" ]; then
            tag="$prog"
        fi
        args="build-$prog $defs"
    fi

    build_run "$tag" "$args"
    check_result 'check result' "$check" "$@"
}

## kernel image
osimg=$(make_print ucoreimg)

## swap image
swapimg=$(make_print swapimg)

## set default qemu-options
qemuopts="-machine virt -nographic -bios default -device loader,file=bin/ucore.img,addr=0x80200000"

## set break-function
## 默认不使用 GDB（避免因连接/断点问题导致卡死或过早退出）。
## 如需恢复原行为：COW_GRADE_USE_GDB=1 bash tools/cow_grade.sh
brkfun=
if [ "${COW_GRADE_USE_GDB:-0}" = "1" ]; then
    brkfun=readline
fi

default_check() {
    pts=7
    check_regexps "$@"
}

## check now!!

pts=20
run_test -prog 'cow_test' -check default_check \
    'kernel_execve: pid = 2, name = "cow_test".' \
    'Child: PASSED - COW triggered successfully' \
    'Parent: PASSED - Data isolation successful'

pts=20
run_test -prog 'dirtycow_defense_test' -check default_check \
    'kernel_execve: pid = 2, name = "dirtycow_defense_test".' \
    'Test 2: Verify permission check during COW' \
    'Parent: PASSED - Data unchanged'

show_final
