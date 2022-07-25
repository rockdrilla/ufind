## About

`ufind` enumerates unique (from `dev_t:ino_t` logic) files and output their names to `stdout`.

`ufind` is small supplemental utility for [minimal Debian container image](https://github.com/rockdrilla/docker-debian) (work in progress) and mostly useful for building container images (`RUN <...>` stanzas in `Dockerfile`/`Containerfile`).

Program is very dumb, so feel free to open issue/PR. :)

---

## Usage:

`ufind [-z] <path> [..<path>]`

### Options:

| Option | Description                            |
| ------ | -------------------------------------- |
|  `-z`  | separate entries with \0 instead of \n |

### Environment variables:

| Variable          | Value           | Description                                                  |
| ----------------- | --------------- | ------------------------------------------------------------ |
| `UFIND_USE_LISTS` | (any non-empty) | use work lists instead of stack recursion - slower but safer |

## Pros:

- beats shell script variant by speed (see below)
- much smaller than [sharkdp/fd](https://github.com/sharkdp/fd) :)
- supports \0 delimiter in output ([sharkdp/fd](https://github.com/sharkdp/fd) as of version 8.4.0 - does not)

## Cons:

- error-prone to TOCTOU (insanely large window), that's why it's safely usable only while building container images
- much slower than [sharkdp/fd](https://github.com/sharkdp/fd) :(

## Building from source:

Only "standard" things are required: binutils, gcc and libc6-dev.

`gcc -o ufind prog.c`

## Shell script alternative:

simple but slow

```sh
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
```

---

## License

BSD 3-Clause
- [spdx.org](https://spdx.org/licenses/BSD-3-Clause.html)
- [opensource.org](https://opensource.org/licenses/BSD-3-Clause)
