<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)"  srcset=".github/mine-banner-dark.png">
    <source media="(prefers-color-scheme: light)" srcset=".github/mine-banner-light.png">
    <img alt="Mine Text Editor Banner" src=".github/mine-banner-light.png">
  </picture>
</p>

## Building from Source

Mine is built using the [build2](https://build2.org/build2-toolchain/doc/build2-toolchain-intro.xhtml#preface) build system. To get started, you'll need the staged toolchain, which you can grab as either source or binary packages. The pre-built binaries are available here:

* [Staged Toolchain](https://stage.build2.org/0/0.18.0-a.0/bindist/)
* [SHA256 Checksums](https://stage.build2.org/0/toolchain-bindist.sha256)

You'll also need a working GCC compiler. Use GCC if you're compiling on Linux, or MinGW if you're on Windows. If you need help setting these up, you can find installation instructions below:

* [GCC](https://gcc.gnu.org/)
* [MinGW-w64](https://www.mingw-w64.org/)

### Consumption

> [!NOTE]
> **Consumption** is just `build2`'s term for building the package without actively modifying the code. These steps are great if you want to test the latest development builds, but we don't recommend them for regular users just looking to install and use Mine.

#### Windows

```powershell
# Create the build configuration:
#
bpkg create -d mine -@mingw32       ^
  config.cxx=x86_64-w64-mingw32-g++ ^
  cc                                ^
  config.bin.lib=static             ^
  config.install.root=C:/mine/      ^
  
cd mine

# To build:
#
bpkg build mine@https://github.com/wroyca/mine.git#main

# To test:
#
bpkg test mine

# To install:
#
bpkg install mine

# To uninstall:
#
bpkg uninstall mine

# To upgrade:
#
bpkg fetch
bpkg status mine
bpkg uninstall mine
bpkg build --upgrade --recursive mine
bpkg install mine
```

#### Linux

```bash
# Create the build configuration:
#
bpkg create -d mine @gcc    \
  cc                        \
  config.bin.lib=static     \
  config.install.root=/usr  \
  config.install.sudo=sudo

cd mine

# To build:
#
bpkg build mine@https://github.com/wroyca/mine.git#main

# To test:
#
bpkg test mine

# To install:
#
bpkg install mine

# To uninstall:
#
bpkg uninstall mine

# To upgrade:
#
bpkg fetch
bpkg status mine
bpkg uninstall mine
bpkg build --upgrade --recursive mine
bpkg install mine
```

If you want to dive deeper into how these commands work, check out the [build2 package consumption guide](https://build2.org/build2-toolchain/doc/build2-toolchain-intro.xhtml#guide-consume-pkg).

### Development

> [!NOTE]
> These instructions are meant for developers looking to modify and contribute to Mine's codebase. If you just want to compile and test the latest build, follow the **Consumption** steps above instead.

#### Linux

```bash
# Clone the repository:
#
git clone https://github.com/wroyca/mine.git

cd mine

# Create the build configuration:
#
bdep init -C @gcc                                   \
  cc                                                \
  config.cc.compiledb=./                            \
  config.cc.coptions="-ggdb                         \
                      -grecord-gcc-switches         \
                      -fno-omit-frame-pointer       \
                      -mno-omit-leaf-frame-pointer"
  
# To build:
#
b

# To test:
#
b test
```

#### Windows

```powershell
# Clone the repository:
#
git clone https://github.com/wroyca/mine.git

cd mine

# Create the build configuration:
#
bdep init -C -@mingw32                              ^
  config.cxx=x86_64-w64-mingw32-g++                 ^
  cc                                                ^
  config.cc.compiledb=./                            ^
  config.cc.coptions="-ggdb                         ^
                      -grecord-gcc-switches         ^
                      -fno-omit-frame-pointer       ^
                      -mno-omit-leaf-frame-pointer"

# To build:
#
b

# To test:
#
b test
```

For more details on `bdep` and typical development workflows, take a look at the [build2 build system manual](https://build2.org/build2/doc/build2-build-system-manual.xhtml).

## Contributing

Contributions are welcome! If you are interested in helping expand Mine's editing facilities, please feel free to submit a Pull Request or open an Issue to discuss architectural changes.

## License

Mine is released under the [GNU General Public License v3](LICENSE.md).

