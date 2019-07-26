CURRENT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
FUCHSIA_DIR="$CURRENT_DIR"/../..

OUTPUT_DIR=$FUCHSIA_DIR/out/default
ZIRCON_OUTPUT_DIR=$FUCHSIA_DIR/out/default.zircon

$FUCHSIA_DIR/.jiri_root/bin/fx metrics disable

QEMU_TEST_CMDS=/tmp/runcmds

ALL_TESTS=/tmp/tests.json
QEMU_TESTS=/tmp/qemu-tests
HOST_TESTS=/tmp/host-tests.json
QEMU_HOST_RESULTS_DIR=`mktemp -d`

JIRI=$FUCHSIA_DIR/.jiri_root/bin/jiri
FX=$FUCHSIA_DIR/.jiri_root/bin/fx
ZBI=$ZIRCON_OUTPUT_DIR/tools/zbi
MINFS=$ZIRCON_OUTPUT_DIR/tools/minfs
RUN_ZIRCON=$FUCHSIA_DIR/zircon/scripts/run-zircon-x64
TESTSHARDER=$FUCHSIA_DIR/buildtools/linux-x64/testsharder
TESTRUNNER=$FUCHSIA_DIR/buildtools/linux-x64/testrunner

QEMU_ZBI=/tmp/fuchsia-qemu.zbi
OUTPUT_FS=/tmp/output.fs
HOST_ARCHIVE=/tmp/host-archive.tar