#!/bin/sh
set -e
srcdir="$(dirname $0)"
autoreconf --install --force --warnings=all
