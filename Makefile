PREFIX ?= /usr/local
LIBDIR ?= $(PREFIX)/lib
PLUGIN_DIR ?= $(LIBDIR)/gstreamer-1.0
CARGO ?= cargo
PROFILE ?= release
DESTDIR ?=

# Rust plugin crates, one library name each.
RUST_PLUGINS := gsthype gstinjectbin

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  LIB_PREFIX := lib
  SRC_EXT := dylib
  DEST_EXT := so
else ifeq ($(OS),Windows_NT)
  LIB_PREFIX :=
  SRC_EXT := dll
  DEST_EXT := dll
else
  LIB_PREFIX := lib
  SRC_EXT := so
  DEST_EXT := so
endif

.PHONY: all build test fmt clippy install clean

all: build

build:
	$(CARGO) build --workspace --profile $(PROFILE)

test:
	$(CARGO) test --workspace

fmt:
	$(CARGO) fmt --all

clippy:
	$(CARGO) clippy --workspace --all-targets -- -D warnings

install: build
	install -d "$(DESTDIR)$(PLUGIN_DIR)"
	@for p in $(RUST_PLUGINS); do \
		echo "Installing $$p"; \
		install -m 755 "target/$(PROFILE)/$(LIB_PREFIX)$$p.$(SRC_EXT)" \
			"$(DESTDIR)$(PLUGIN_DIR)/$(LIB_PREFIX)$$p.$(DEST_EXT)"; \
	done

clean:
	$(CARGO) clean
