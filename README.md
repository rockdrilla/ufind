## About

`ufind` enumerates unique files and output their names to `stdout`.

`ufind` is small supplemental utility for [minimal Debian container image](https://github.com/rockdrilla/docker-debian) (*work in progress*) and mostly useful for building container images (`RUN <...>` stanzas in `Dockerfile`/`Containerfile`).

Program is kinda *dumb* so feel free to open issue/PR. :)

---

## How it works:

`ufind` treats `dev_t:ino_t` as "unique identifier" for files and directories.

`ufind` iterates through each argument:

- argument is symlink:
  - determine underlying file system object type with `open(2)`/`fstat(2)`
  - call appropriate handler

> `ufind` handles command line arguments as they were symlinks.

- argument is neither regular file nor directory (e.g. devices, sockets, etc.):
  - print warning to `stderr`

- argument is regular file:
  - it's identifier is seen first time:
    - remember identifier
    - print (full) file name to `stdout`
  - it's identifier is already known:
    - do nothing (file is already handled)

- argument is directory:
  - it's identifier is seen first time:
    - remember identifier
    - iterate it's members and handle them accordingly
  - it's identifier is already known:
    - do nothing (directory is already handled)

Nota bene: there's no sorting of any kind (directory entries or output)
so consecutive calls against same file system MAY produce different output.

### Example:

Consider following tree:

```sh
  $ find * | sort -V | xargs -r stat -c '%A %d:%i %N'

  drwxr-xr-x 36:31 'dir1'
  -rw-r--r-- 36:34 'dir1/file1'
  lrwxrwxrwx 36:41 'dir1/link1' -> 'file1'
  lrwxrwxrwx 36:44 'dir1/link2' -> '../dir2/dir3/file2'
  drwxr-xr-x 36:32 'dir2'
  drwxr-xr-x 36:33 'dir2/dir3'
  -rw-r--r-- 36:36 'dir2/dir3/file2'
  -rw-r--r-- 36:34 'dir2/dir3/hardlink2'
  lrwxrwxrwx 36:28 'dir2/dir3/link3' -> '../file3'
  -rw-r--r-- 36:27 'dir2/file3'
  -rw-r--r-- 36:27 'dir2/hardlink1'
```

`ufind` produces following output:

```sh
  $ ufind *

  /tmp/ufind-example/dir1/file1
  /tmp/ufind-example/dir2/hardlink1
  /tmp/ufind-example/dir2/dir3/file2

  $ ufind * | sort -V | xargs -r stat -c '%A %d:%i %N'

  -rw-r--r-- 36:34 '/tmp/ufind-example/dir1/file1'
  -rw-r--r-- 36:36 '/tmp/ufind-example/dir2/dir3/file2'
  -rw-r--r-- 36:27 '/tmp/ufind-example/dir2/hardlink1'
```

As seen, output MAY be totally unsorted and a bit confusing:

- `'dir2/hardlink1'` was printed instead of `'dir2/file3'` because it has "arrived" first (both files are "hardlinks");
- `'dir2/dir3/file2'` was printed after `'dir2/hardlink1'`.

## Usage:

`ufind [-z] <path> [..<path>]`

### Options:

| Option | Description                            |
| ------ | -------------------------------------- |
|  `-z`  | separate entries with \0 instead of \n |

## Pros:

- beats shell script variant by speed (see below)
- much smaller than [sharkdp/fd](https://github.com/sharkdp/fd) :)

```sh
  $ stat -Lc $'%s\t%n' ufind /usr/bin/fdfind
  31296   ufind
  2501320 /usr/bin/fdfind
```

## Cons:

- error-prone to TOCTOU, that's why it's safely usable only while building container images
- much slower than [sharkdp/fd](https://github.com/sharkdp/fd) :(
- totally unsorted output

## Benchmark:

```sh
$ ./benchmark.sh

find /usr/share/: 283564
./ufind-terse.sh /usr/share/: 235372
./ufind /usr/share/: 235372
fdfind -u -j 1 . /usr/share/: 283563
fdfind -u . /usr/share/: 283563

Benchmark 1: ./ufind-terse.sh /usr/share
  Time (mean ± σ):      3.791 s ±  0.037 s    [User: 3.685 s, System: 0.580 s]
  Range (min … max):    3.740 s …  3.860 s    10 runs

Benchmark 1: ./ufind /usr/share
  Time (mean ± σ):     534.5 ms ±  23.6 ms    [User: 346.0 ms, System: 187.0 ms]
  Range (min … max):   515.9 ms … 586.7 ms    50 runs

Benchmark 1: fdfind -u -j 1 . /usr/share
  Time (mean ± σ):     238.1 ms ±   3.2 ms    [User: 179.0 ms, System: 139.1 ms]
  Range (min … max):   232.6 ms … 246.7 ms    50 runs

Benchmark 1: fdfind -u . /usr/share
  Time (mean ± σ):     112.8 ms ±   2.3 ms    [User: 460.2 ms, System: 855.0 ms]
  Range (min … max):   109.8 ms … 126.9 ms    50 runs
```

## Building from source:

Note: clone repository **recursively** (due to `git submodule`)

```sh
git clone --recurse-submodules https://github.com/rockdrilla/ufind.git
```

Build dependencies: `gcc` and `libc6-dev`.

*They're quite common for building software from sources.*

```sh
gcc -O2 -o ufind ufind.c
```

## Shell script alternative:

simple but slow (ref: [`ufind-terse.sh`](ufind-terse.sh))

```sh
#!/bin/sh
find "$@" -follow -type f -print0 \
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
