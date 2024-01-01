#!/bin/sh
# Run from source directory, e.g. ./resources/oc_version.sh "1.0.0"

if [ -z "$1" ]; then
	echo "Please specify version string"
	exit 1
fi

cat > OC_version.h <<EOF
// NOTE: DO NOT INCLUDE DIRECTLY, USE OC::Strings::VERSION
"$1"
EOF
