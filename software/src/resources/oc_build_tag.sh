#!/bin/sh

git_rev="$(git rev-parse --short HEAD)"
git_status="$(git status -s --untracked-files=no)"

if [ -n "$git_status" ] ; then
	suffix="dirty"
fi

echo "${git_rev}${suffix}"
