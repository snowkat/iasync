# iasync - sync files to iOS app folders

**iasync** uses [libimobiledevice] to sync a local directory tree with an iOS device. This is intended as a replacement to pairing ifuse and rsync, where users might have issues with fuse (or be unable to use fuse at all).

## Building

This project requires the following:

- [libimobiledevice] for the actual iOS bits.
- [libbsd] on non-BSD/macOS systems.
- [meson] and [ninja] as the build system.

To build in the directory `build`, run:

```
meson setup build/
ninja -C build/
# To install:
ninja -C build/ install
```

## Usage

See the manpage for more details (`man 1 iasync`).

Primarily, if you have more than one iOS device connected to your system, you'll need the name or UDID of your device:

```
$ iasync lsdevs
Name        Conn.      UDID
My Iphone   USB        00000000-0000000
```

Then, get the Bundle ID for the app you want to sync:

```
$ iasync lsapps
App Name   Bundle ID
[...]
Doppler    co.brushedtype.doppler-ios
```

To sync a directory tree to the app, you could run:

```
$ iasync sync ~/Music co.brushedtype.doppler-ios
```

If you want to delete files that exist on the device but not locally, use the `--allow-delete` flag:

```
$ iasync sync --allow-delete ~/Music co.brushedtype.doppler-ios
```

## License

iasync is provided under the BSD 3-Clause license. For more information, please see the LICENSE file.

Multiple functions (`idevfs_canonpath()` and `idevfs_mkpath()`) use code from [NetBSD]. These licenses are included in the NOTICES file.

[libimobiledevice]: https://libimobiledevice.org/
[libbsd]: https://libbsd.freedesktop.org/wiki/
[meson]: https://mesonbuild.com/
[ninja]: https://ninja-build.org/
[NetBSD]: https://www.netbsd.org
