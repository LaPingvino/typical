#!/bin/sh
# typical matching harness.
#
# Runs every request in requests.txt through fc-match (or fc-match -s)
# against the fonts installed on this machine, using the fontconfig built
# in $BUILD, and writes a plain-text result table.  Commit one run as a
# baseline; after a patch, run again and diff.
#
#   test/typical/run.sh [-b BUILD_DIR] [-o OUT] [-s]
#     -b  meson build dir (default: $TMPDIR/typ-build)
#     -o  output file (default: stdout)
#     -s  use the system fc-match instead (for comparison)
#
#   test/typical/run.sh -o /tmp/after.txt && diff -u test/typical/baseline.txt /tmp/after.txt
#
# The environment is pinned so results do not depend on the caller's locale
# or on the build's --prefix: the system's /etc/fonts configuration is used
# (FONTCONFIG_PATH makes the relative <include>conf.d</include> resolve),
# and the default language is "en".
set -eu

here=$(dirname "$0")
build=${TMPDIR:-/tmp}/typ-build
out=
system=no
while getopts b:o:s opt; do
    case $opt in
    b) build=$OPTARG ;;
    o) out=$OPTARG ;;
    s) system=yes ;;
    *) echo "usage: $0 [-b BUILD_DIR] [-o OUT] [-s]" >&2; exit 2 ;;
    esac
done

if [ $system = yes ]; then
    fcmatch=fc-match
else
    fcmatch=$build/fc-match/fc-match
    [ -x "$fcmatch" ] || { echo "no fc-match in $build; build first" >&2; exit 1; }
    LD_LIBRARY_PATH=$build${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
    export LD_LIBRARY_PATH
fi

FONTCONFIG_FILE=/etc/fonts/fonts.conf
FONTCONFIG_PATH=/etc/fonts
FC_LANG=en
LC_ALL=C.UTF-8
export FONTCONFIG_FILE FONTCONFIG_PATH FC_LANG LC_ALL
unset FC_DEBUG 2>/dev/null || true

fmt='%{family[0]} | %{style[0]} | weight=%{weight} width=%{width} slant=%{slant} | %{file|basename}#%{index}\n'

run() {
    # Fingerprint of the font set, so baselines from different machines or
    # font installs are not compared by accident.
    nfaces=$("$fcmatch" -a -f '%{file}#%{index}\n' : | wc -l)
    fsum=$("$fcmatch" -a -f '%{file}#%{index}\n' : | LC_ALL=C sort | sha256sum | cut -c1-16)
    echo "# typical baseline: $nfaces faces, font-set sha256 $fsum"
    echo "# fc-match: $fcmatch"
    grep -v '^[[:space:]]*\(#\|$\)' "$here/requests.txt" | while IFS= read -r line; do
        kind=${line%% *}
        rest=${line#* }
        case $kind in
        match)
            printf '%s\n    %s' "match $rest" ''
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
