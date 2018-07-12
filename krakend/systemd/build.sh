#!/bin/sh
set -e

RUN_DIR="`pwd`"
SCRIPT_FILE="`readlink -f "${0}"`"
SCRIPT_DIR="`dirname "${SCRIPT_FILE}"`"

cd "${SCRIPT_DIR}"

mkdir -p target/

for source in src/*
do
    output="target/`basename "${source}" .sh`"
    "./${source}" > "${output}"
done

cd "${RUN_DIR}"
