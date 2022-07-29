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

## Pros:

- beats shell script variant by speed (see below)
- much smaller than [sharkdp/fd](https://github.com/sharkdp/fd) :)

## Cons:

- error-prone to TOCTOU, that's why it's safely usable only while building container images
- much slower than [sharkdp/fd](https://github.com/sharkdp/fd) :(

## Benchmark:

```sh
$ ./benchmark.sh

find /usr/share/: 283379
./ufind-terse.sh /usr/share/: 235382
./ufind /usr/share/: 235382
fdfind -u -j 1 . /usr/share/: 283378
fdfind -u . /usr/share/: 283378

Benchmark 1: ./ufind-terse.sh /usr/share
  Time (mean ± σ):      3.791 s ±  0.025 s    [User: 3.793 s, System: 0.655 s]
  Range (min … max):    3.757 s …  3.821 s    10 runs

Benchmark 1: ./ufind /usr/share
  Time (mean ± σ):     693.0 ms ±  28.9 ms    [User: 509.9 ms, System: 181.6 ms]
  Range (min … max):   664.0 ms … 738.1 ms    50 runs

Benchmark 1: fdfind -u -j 1 . /usr/share
  Time (mean ± σ):     245.6 ms ±   5.0 ms    [User: 212.2 ms, System: 174.1 ms]
  Range (min … max):   239.1 ms … 259.1 ms    50 runs

Benchmark 1: fdfind -u . /usr/share
  Time (mean ± σ):     120.0 ms ±   1.0 ms    [User: 535.8 ms, System: 1021.7 ms]
  Range (min … max):   116.9 ms … 121.5 ms    50 runs
```

## Building from source:

Only "standard" things are required: binutils, gcc and libc6-dev.

`gcc -o ufind prog.c`

## Shell script alternative:

simple but slow (ref: [`ufind-terse.sh`](ufind-terse.sh))

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

Apache-2.0

- [spdx.org](https://spdx.org/licenses/Apache-2.0.html)
- [opensource.org](https://opensource.org/licenses/Apache-2.0)
- [apache.org](https://www.apache.org/licenses/LICENSE-2.0)
