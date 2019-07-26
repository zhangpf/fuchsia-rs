#!/bin/bash
set -e
set -x trace
set -o pipefail


CURRENT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

. "$CURRENT_DIR"/env.sh

cd $FUCHSIA_DIR

cat > $QEMU_TEST_CMDS << EOF

mkdir /tmp/infra-test-output
waitfor class=block topo=/dev/sys/pci/00:06.0/virtio-block/block timeout=60000
mount /dev/sys/pci/00:06.0/virtio-block/block /tmp/infra-test-output 
runtests -o /tmp/infra-test-output -f /boot/infra/tests.lst || echo "1" > /tmp/infra-test-output/result
echo "0" > /tmp/infra-test-output/result 
umount /tmp/infra-test-output
dm poweroff
EOF

buildtools/linux-x64/testsharder -build-dir out/default -output-file $ALL_TESTS
jq -r '.[] | select(.name == "QEMU") | .tests | .[] | .location' $ALL_TESTS > $QEMU_TESTS

$ZBI -o $QEMU_ZBI out/default/fuchsia.zbi \
	-e "infra/runcmds=$QEMU_TEST_CMDS" -e "infra/tests.lst=$QEMU_TESTS"

$MINFS "$OUTPUT_FS"@1G create

$FX run -k -c "zircon.autorun.system=/boot/bin/sh+/boot/infra/runcmds" -m 4096 -z $QEMU_ZBI -- \
	-drive file=$OUTPUT_FS,format=raw,if=none,id=testdisk -device virtio-blk-pci,drive=testdisk,addr=06.0
$MINFS $OUTPUT_FS cp :: $QEMU_HOST_RESULTS_DIR

test -f $QEMU_HOST_RESULTS_DIR/result  
cat $QEMU_HOST_RESULTS_DIR/result | xargs test 0 = 
! cat $QEMU_HOST_RESULTS_DIR/summary.json | grep FAIL > /dev/null