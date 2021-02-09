# BMFontToIrrlicht

Convert a BMFont font to an Irrlicht XML font.

## Compiling

The script needs a C++17 compatible compiler and optionally CMake.

With CMake:
```
mkdir build
cd build
cmake ..
make
```

Without CMake:
```
g++ -std=c++17 main.cpp -o BMFontToIrrlicht
```

## Running

```
./BMFontToIrrlicht input.fnt output.xml
```

## Limitations

This has only been tested for fonts created by LibGDX's Hiero.

Missing support for:
- Padding
- Pages
- Channels
- Non-ascii chars
- Proper overhang/underhang
