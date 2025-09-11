#!/usr/bin/env bash
# usage: ./char_stats.sh <file>

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <file>" >&2
    exit 1
fi

file=$1

# feed the raw bytes as decimal codes into awk
od -An -t u1 -v "$file" |
awk '
{
    for (i = 1; i <= NF; i++) {
        b = $i
        bytes++
        if (b <= 127) ascii++
        if (b >= 65 && b <= 90) upper++
        else if (b >= 97 && b <= 122) lower++
        if (b >= 48 && b <= 57) digit++
        if (b == 32) space++
    }
}
END {
    printf "Bytes: %d\n", bytes
    printf "ASCII: %d\n", ascii
    printf "Uppercase: %d\n", upper
    printf "Lowercase: %d\n", lower
    printf "Digits: %d\n", digit
    printf "Spaces: %d\n", space
}'
