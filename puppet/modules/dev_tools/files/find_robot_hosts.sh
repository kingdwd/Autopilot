#!/bin/bash
# http://stackoverflow.com/a/25277451

host_to_find="[d|i][0-9]+"

while read ip host; do
    echo "$host"
done < <(awk "/^[[:space:]]*($|#)/{next} /$host_to_find/{print \$1 \" \" \$2}" /etc/hosts)

