#!/bin/bash

VERSION="$(git describe --tags --dirty --always)"
LONGNAME=ubuntu-arale-kernel-$VERSION

git archive --format tgz --prefix=$LONGNAME/ -o ../$LONGNAME.tgz master

