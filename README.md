# lqr-simple

Fork of [liblqr](https://github.com/carlobaldassi/liblqr) but without the glib-2.0 dependency.

## Requirements

* C11 complier (gcc recommended)
* CMake at least 3.1

## Building

```
mkdir build
cd build
cmake ..
make
```

## Adding to your cmake project

To add lqr-simple to your project simply put these lines to your CMakeLists.txt:

```
find_package(lqr-simple REQUIRED)
target_include_directories(yourproject PUBLIC lqr-simple)
target_link_libraries(yourproject lqr-simple)
```

## Library reference

Please see upstream folder `examples`: https://github.com/carlobaldassi/liblqr/tree/master/examples

## TODO

* Fix building on Windows
