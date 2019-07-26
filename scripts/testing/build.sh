#!/bin/bash
set -e
set -x
set -o pipefail

CURRENT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

. "$CURRENT_DIR"/env.sh

if [ $# -eq 1 ]
 then
    if [ $1 = "core" ]; then
    	product="core"
	elif [ $1 = "bringup" ]; then
		product="bringup"
	elif [ $1 = "terminal" ]; then
		product="terminal"
	elif [ $1 = "workstation" ]; then
		product="workstation"
	else 
		>&2 echo "not a valid product name, must be core,"\
		          "bringup, terminal or workstation."
		exit 1
	fi
else 
	>&2 echo "not a valid product name, must be core,"\
		          "bringup, terminal or workstation."
	exit 1
fi

cd $FUCHSIA_DIR
$FX set "$product".x64 --args base_package_labels+=[\"//bundles/buildbot:"$product"\"]
$FX build
