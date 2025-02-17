# libmine - A C++ library

The `libmine` C++ library provides <SUMMARY-OF-FUNCTIONALITY>.


## Usage

To start using `libmine` in your project, add the following `depends`
value to your `manifest`, adjusting the version constraint as appropriate:

```
depends: libmine ^<VERSION>
```

Then import the library in your `buildfile`:

```
import libs = libmine%lib{<TARGET>}
```


## Importable targets

This package provides the following importable targets:

```
lib{<TARGET>}
```

<DESCRIPTION-OF-IMPORTABLE-TARGETS>


## Configuration variables

This package provides the following configuration variables:

```
[bool] config.libmine.<VARIABLE> ?= false
```

<DESCRIPTION-OF-CONFIG-VARIABLES>
