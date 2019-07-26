#!/bin/bash
set -e
set -x
set -o pipefail

CURRENT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

. "$CURRENT_DIR"/env.sh

if [ ! -z ${JENKINS_HOME+x} ]; 
then

	echo "here"
	cd $FUCHSIA_DIR
	curl -s "https://fuchsia.googlesource.com/jiri/+/master/scripts/bootstrap_jiri?format=TEXT" | base64 --decode | bash -s $FUCHSIA_DIR

	$JIRI import -name=integration -remote-branch fuchsia-rs \
		flower https://github.com/fuchsia-rs/integration
	$JIRI override -path=unused fuchsia https://github.com/fuchsia-rs/fuchsia-rs
	$JIRI update
	$JIRI override -delete fuchsia
	$JIRI run-hooks
else
	echo "currently not in jenkins environment."
fi
