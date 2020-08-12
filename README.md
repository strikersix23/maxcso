Description
===========

A fast ISO to CSO compression program for use with PSP and PS2 emulators, which uses multiple
algorithms for best compression ratio.


Basic usage
===========

```sh
maxcso myfile.iso
```

Or, drag the iso file into maxcso.exe on Windows.


Release
===========

Get the latest release from the [GitHub releases][].  Use `maxcso.exe` on modern systems, and use
`maxcso32.exe` on Windows XP and other 32-bit versions of Windows.


Features
===========

  * Can use as many CPU cores as you want.
  * Can use [zlib][], [7-zip][]'s deflate, and [Zopfli][].
  * Processes multiple files in one command.
  * Can take a CSO or DAX file as a source.
  * Able to output at larger block sizes.
  * Support for experimental [CSO v2][] and [ZSO][] formats using [lz4][] (faster decompression.)
  * Tuning of deflate or lz4 compression threshold.
  * Decompression of all supported inputs (including DAX and CSO v2.)


Compression
===========

maxcso always uses compression level 9.  Decompression speed is about the same regardless of
level, and disk access is faster with smaller files.

Using 7-zip's deflate and Zopfli improves compression ratios, but don't expect a lot.  Usual
results are between 0.5% to 1.0% smaller.

Larger block sizes than the default will help compression, in the range of 2-3%.  However, the
files may not be compatible with some software.  For example, [PPSSPP][] versions released
after 2014-10-26 will support larger block sizes.

Avoid DAX where CSOs using larger block sizes are supported, since DAX is less efficient.

LZ4 support is mostly for experimentation.


Speed
===========

Compared to other tools like ciso and CisoPlus, maxcso can run much faster and achieve the same
compression.  Use `--fast` to get the fastest compression, which matches level 9 in other tools.

Additionally, if you have better than a dual core processor, maxcso will use all of your cores,
and perform even better.

In usage, CSOs typically perform well in all known emulators.  Some versions of PSP firmware with
support for CSOs have bugs in their CSO support, but this doesn't affect emulators.


Full program usage
===========

```
Usage: maxcso [--args] input.iso [-o output.cso]

Multiple files may be specified.  Inputs can be iso or cso files.

   --threads=N     Specify N threads for I/O and compression
   --quiet         Suppress status output
   --crc           Log CRC32 checksums, ignore output files and methods
   --fast          Use only basic zlib or lz4 for fastest result
   --decompress    Write out to raw ISO, decompressing as needed
   --block=N       Specify a block size (default depends on iso size)
                   Many readers only support the 2048 size
   --format=VER    Specify cso version (options: cso1, cso2, zso, dax)
                   These are experimental, default is cso1
   --use-zlib      Enable trials with zlib for deflate compression
   --use-zopfli    Enable trials with Zopfli for deflate compression
   --use-7zdeflate Enable trials with 7-zip's deflate compression
   --use-lz4       Enable trials with lz4hc for lz4 compression
   --use-lz4brute  Enable bruteforce trials with lz4hc for lz4 compression
   --only-METHOD   Only allow a certain compression method (zlib, etc. above)
   --no-METHOD     Disable a certain compression method (zlib, etc. above)
                   The default is to use zlib and 7zdeflate only
   --lz4-cost=N    Allow lz4 to increase block size by N% at most (cso2 only)
   --orig-cost=N   Allow uncompressed to increase block size by N% at most
   --output-path=X Output to path X/, use basename for default outputs
```

Because Zopfli is significantly slower than the other methods, and uses a lot more memory, it
is disabled by default.  Add `--use-zopfli` for maximum compression.

The cost arguments allow you to allow each block to be N% bigger by using lz4 or no
compression.  This makes the file read faster (less cpu power), but take more space.


Platforms
===========

maxcso has been tested on Windows, macOS, and Linux so far.  The code was written to be portable.
If you'd like to port it to another platform, pull requests are accepted.  It may just compile
without any changes.

### Windows

To build on Windows, simply open cli/maxcso.sln and build.  Visual Studio 2017 is required.

### Mac OS X

Aside from gcc/g++ or clang (from Xcode or brew), you will also need:

    brew install lz4
    brew install libuv

And then just compile using make.

### Linux / Unix

Aside from gcc/g++ or clang, you will also need liblz4-dev and libuv1-dev, or similar.


Credits and licensing
===========

The larger portion of code here is from others' wonderful work in decompression and I/O
libraries.  Licensing is as follows:

 * maxcso is licensed under ISC.
 * [7-zip][] and [p7zip][] are licensed under LGPL.
 * [Zopfli][] is licensed under Apache 2.0.
 * [libuv][] is licensed under MIT.
 * [zlib][] is licensed under zlib.
 * [lz4][] is licensed under BSD.


Other tools
===========

 * [CisoPlus][] by kapoue3
 * [CisoMC][] by LMAN
 * [ciso][] by BOOSTER
 * [ciso-python][] by Virtuous Flame


[zlib]: https://github.com/madler/zlib
[7-zip]: http://7-zip.org/
[p7zip]: http://p7zip.sourceforge.net/
[Zopfli]: http://code.google.com/p/zopfli/
[PPSSPP]: https://github.com/hrydgard/ppsspp
[libuv]: https://github.com/joyent/libuv
[CisoPlus]: http://cisoplus.pspgen.com/
[CisoMC]: http://wololo.net/talk/viewtopic.php?f=20&t=32659
[ciso]: http://sourceforge.net/projects/ciso/
[ciso-python]: http://virtuousflame.blog.163.com/blog/static/177177172201111833413485/
[lz4]: https://code.google.com/p/lz4/
[CSO v2]: README_CSO.md
[ZSO]: README_ZSO.md
[GitHub releases]: https://github.com/unknownbrackets/maxcso/releases
