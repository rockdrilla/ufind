#!/bin/sh
for i ; do
    [ -n "$i" ] || continue
    if [ -f "$i" ] ; then
        printf '%s\0' "$i"
    elif [ -d "$i" ] ; then
        find "$i/" -follow -type f -print0
    fi
done \
| xargs -0 -r -n 64 stat -L --printf='%d:%i|%n\0' \
| sort -z -u -t '|' -k1,1 \
| cut -z -d '|' -f 2