# libmine-terminal - A C++ library

The `libmine-terminal` C++ library provides <SUMMARY-OF-FUNCTIONALITY>.


## Usage

To start using `libmine-terminal` in your project, add the following `depends`
value to your `manifest`, adjusting the version constraint as appropriate:

```
depends: libmine-terminal ^<VERSION>
```

Then import the library in your `buildfile`:

```
import libs = libmine-terminal%lib{<TARGET>}
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
[bool] config.libmine_terminal.<VARIABLE> ?= false
```

<DESCRIPTION-OF-CONFIG-VARIABLES>
