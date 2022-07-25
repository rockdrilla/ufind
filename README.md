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

## Cons:

- error-prone to TOCTOU (insanely large window), that's why it's safely usable only while building container images
- much slower than [sharkdp/fd](https://github.com/sharkdp/fd) :(

## Benchmark:

```sh
$ ./benchmark.sh

find /usr/share/: 283348
./ufind-terse.sh /usr/share/: 235353
./ufind /usr/share/: 235353
./ufind /usr/share/: 235353
fdfind -u -j 1 . /usr/share/: 283347
fdfind -u . /usr/share/: 283347

Benchmark 1: ./ufind-terse.sh /usr/share
  Time (mean ± σ):     10.559 s ±  0.075 s    [User: 9.500 s, System: 3.019 s]
  Range (min … max):   10.422 s … 10.713 s    10 runs

Benchmark 1: ./ufind /usr/share
  Time (mean ± σ):      1.710 s ±  0.055 s    [User: 0.759 s, System: 0.942 s]
  Range (min … max):    1.643 s …  1.808 s    50 runs

Benchmark 1: ./ufind /usr/share
  Time (mean ± σ):      2.047 s ±  0.058 s    [User: 0.771 s, System: 1.265 s]
  Range (min … max):    1.960 s …  2.142 s    50 runs

Benchmark 1: fdfind -u -j 1 . /usr/share
  Time (mean ± σ):     269.5 ms ±  13.9 ms    [User: 233.6 ms, System: 192.4 ms]
  Range (min … max):   245.0 ms … 318.2 ms    50 runs

Benchmark 1: fdfind -u . /usr/share
  Time (mean ± σ):     119.6 ms ±   0.8 ms    [User: 502.2 ms, System: 1052.1 ms]
  Range (min … max):   118.3 ms … 121.5 ms    50 runs
```

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
