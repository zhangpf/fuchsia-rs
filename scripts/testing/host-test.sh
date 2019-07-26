#!/bin/bash
set -e
set -x trace
set -o pipefail


CURRENT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

. "$CURRENT_DIR"/env.sh

cd $FUCHSIA_DIR

$TESTSHARDER -build-dir out/default -output-file $ALL_TESTS
jq '.[] | select(.name == "Linux") | .tests' $ALL_TESTS -a > $HOST_TESTS

$TESTRUNNER -C out/default -archive $HOST_ARCHIVE $HOST_TESTS

SUMMARY=`tar -axf $HOST_ARCHIVE summary.json -O`

! echo $SUMMARY | grep FAIL > /dev/null