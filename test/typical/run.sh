#!/bin/sh
# typical matching harness.
#
# Runs every request in requests.txt through fc-match (or fc-match -s)
# against the fonts installed on this machine, using the fontconfig built
# in BUILD_DIR, and writes a plain-text result table.  baseline.txt is one
# such run on the unpatched tree; after a patch, run again and diff.
#
#   test/typical/run.sh [-b BUILD_DIR] [-o OUT] [-w WORK_DIR] [-S] [-s]
#     -b  meson build dir (default: $TMPDIR/typ-build)
#     -o  output file (default: stdout)
#     -w  scratch dir for the private config and cache
#         (default: $TMPDIR/typical-env)
#     -S  use the system's conf.d files as they are, without the tree's
#     -s  use the system fc-match, system config and system caches instead
#         (for comparison with what the desktop does today)
#
#   test/typical/run.sh -o $TMPDIR/after.txt
#   diff -u test/typical/baseline.txt $TMPDIR/after.txt
#
# What the default mode does, so that both config and scanner patches show:
#  * config: the set of enabled conf.d files is the system's
#    (/etc/fonts/conf.d), but every file that also exists in the tree's
#    conf.d/ is taken from the tree.  A patch to conf.d/65-nonlatin.conf
#    therefore shows up in the diff.
#  * cache: a private cache dir, rebuilt from scratch with the built
#    fc-cache on every run, so a patch to the scanner (fcfreetype.c,
#    fc-fontations) shows up too.  The system caches are not read.
#  * the default language is pinned to "en" and the locale to C.UTF-8.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
tree=$(cd "$here/../.." && pwd)
build=${TMPDIR:-/tmp}/typ-build
work=${TMPDIR:-/tmp}/typical-env
out=
system=no
treeconf=yes
while getopts b:o:w:Ss opt; do
    case $opt in
    b) build=$OPTARG ;;
    o) out=$OPTARG ;;
    w) work=$OPTARG ;;
    S) treeconf=no ;;
    s) system=yes ;;
    *) echo "usage: $0 [-b BUILD_DIR] [-o OUT] [-w WORK_DIR] [-S] [-s]" >&2; exit 2 ;;
    esac
done

FC_LANG=en
LC_ALL=C.UTF-8
export FC_LANG LC_ALL
unset FC_DEBUG 2>/dev/null || true

if [ $system = yes ]; then
    fcmatch=fc-match
    mode="system fc-match, system config, system caches"
    FONTCONFIG_FILE=/etc/fonts/fonts.conf
    FONTCONFIG_PATH=/etc/fonts
    export FONTCONFIG_FILE FONTCONFIG_PATH
else
    fcmatch=$build/fc-match/fc-match
    fccache=$build/fc-cache/fc-cache
    [ -x "$fcmatch" ] && [ -x "$fccache" ] || { echo "no fc-match/fc-cache in $build; build first" >&2; exit 1; }
    LD_LIBRARY_PATH=$build${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
    export LD_LIBRARY_PATH

    rm -rf "$work"
    mkdir -p "$work/conf.d" "$work/cache"
    for f in /etc/fonts/conf.d/*.conf; do
        name=$(basename "$f")
        if [ $treeconf = yes ] && [ -f "$tree/conf.d/$name" ]; then
            ln -s "$tree/conf.d/$name" "$work/conf.d/$name"
        else
            ln -s "$(readlink -f "$f")" "$work/conf.d/$name"
        fi
    done
    cat > "$work/fonts.conf" <<CONF
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
<fontconfig>
	<dir>/usr/share/fonts</dir>
	<dir>/usr/local/share/fonts</dir>
	<dir prefix="xdg">fonts</dir>
	<dir>~/.fonts</dir>
	<include ignore_missing="yes">$work/conf.d</include>
	<cachedir>$work/cache</cachedir>
</fontconfig>
CONF
    FONTCONFIG_FILE=$work/fonts.conf
    FONTCONFIG_PATH=$work
    export FONTCONFIG_FILE FONTCONFIG_PATH
    "$build/fc-cache/fc-cache" -f >/dev/null 2>&1 || "$build/fc-cache/fc-cache" -f
    if [ $treeconf = yes ]; then
        mode="built library, system conf.d set with tree conf.d files, fresh cache"
    else
        mode="built library, system conf.d files, fresh cache"
    fi
fi

fmt='%{family[0]} | %{style[0]} | weight=%{weight} width=%{width} slant=%{slant} | %{file|basename}#%{index}\n'

run() {
    # Fingerprint of the font set, so baselines from different machines or
    # font installs are not compared by accident.
    nfaces=$("$fcmatch" -a -f '%{file}#%{index}\n' : | wc -l)
    fsum=$("$fcmatch" -a -f '%{file}#%{index}\n' : | LC_ALL=C sort | sha256sum | cut -c1-16)
    echo "# typical baseline: $nfaces faces, font-set sha256 $fsum"
    echo "# mode: $mode"
    grep -v '^[[:space:]]*\(#\|$\)' "$here/requests.txt" | while IFS= read -r line; do
        kind=${line%% *}
        rest=${line#* }
        case $kind in
        match)
            printf '%s\n    ' "match $rest"
            "$fcmatch" -f "$fmt" "$rest"
            ;;
        sort)
            n=${rest%% *}
            pat=${rest#* }
            echo "sort $n $pat"
            "$fcmatch" -s -f "    $fmt" "$pat" | head -n "$n"
            ;;
        *)
            echo "bad line: $line" >&2; exit 1 ;;
        esac
    done
}

if [ -n "$out" ]; then
    run > "$out"
else
    run
fi
