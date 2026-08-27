.POSIX:
.SUFFIXES:
.SUFFIXES: .c .o

include config.mk

vpath %.c src src/input src/output src/tree src/desktop src/ipc
vpath %.h include include/input include/output include/tree include/desktop include/ipc

# flags for compiling
DWLCPPFLAGS = -Iinclude -Iinclude/input -Iinclude/output -Iinclude/tree -Iinclude/desktop -Iinclude/ipc -I. \
	-DWLR_USE_UNSTABLE -D_POSIX_C_SOURCE=200809L \
	-DVERSION=\"$(VERSION)\" $(XWAYLAND)
DWLDEVCFLAGS = -g -Wpedantic -Wall -Wextra -Wdeclaration-after-statement \
	-Wno-unused-parameter -Wshadow -Wunused-macros -Werror=strict-prototypes \
	-Werror=implicit -Werror=return-type -Werror=incompatible-pointer-types \
	-Wfloat-conversion

# CFLAGS / LDFLAGS
PKGS      = wayland-server xkbcommon libinput scenefx-0.4 glesv2 $(XLIBS)
DWLCFLAGS = `$(PKG_CONFIG) --cflags $(PKGS)` $(WLR_INCS) $(DWLCPPFLAGS) $(DWLDEVCFLAGS) $(CFLAGS)
LDLIBS    = `$(PKG_CONFIG) --libs $(PKGS)` $(WLR_LIBS) -lm $(LIBS)

PROTO_HDRS = include/cursor-shape-v1-protocol.h \
	include/pointer-constraints-unstable-v1-protocol.h \
	include/wlr-layer-shell-unstable-v1-protocol.h \
	include/wlr-output-power-management-unstable-v1-protocol.h \
	include/xdg-shell-protocol.h

OBJS = main.o server.o xdg.o layer_shell.o cursor.o seat.o dwl.o util.o rules.o layout.o output.o tree.o workspace.o

all: dwl

dwl: $(OBJS)
	$(CC) $(OBJS) $(DWLCFLAGS) $(LDFLAGS) $(LDLIBS) -o $@

tree-viewer: src/tree_viewer.c
	$(CC) src/tree_viewer.c -Iinclude -I. `pkg-config --cflags raylib wlroots-0.19 wayland-server` `pkg-config --libs raylib wlroots-0.19 wayland-server` -lGL -lm -lpthread -ldl -o $@

main.o: main.c server.h dwl.h include/config.h $(PROTO_HDRS)
server.o: server.c server.h dwl.h tree.h workspace.h rules.h layout.h cursor.h seat.h output.h layers.h util.h client.h include/config.h config.mk $(PROTO_HDRS)
xdg.o: xdg.c xdg.h layer_shell.h dwl.h server.h client.h include/config.h $(PROTO_HDRS)
layer_shell.o: layer_shell.c layer_shell.h dwl.h server.h layout.h client.h include/config.h $(PROTO_HDRS)
cursor.o: cursor.c input/cursor.h dwl.h client.h include/config.h $(PROTO_HDRS)
seat.o: seat.c input/seat.h dwl.h client.h include/config.h $(PROTO_HDRS)
dwl.o: dwl.c server.h dwl.h tree.h workspace.h rules.h layout.h cursor.h seat.h output.h layers.h util.h client.h include/config.h config.mk $(PROTO_HDRS)
util.o: util.c util.h
rules.o: rules.c rules.h dwl.h client.h include/config.h
layout.o: layout.c layout.h tree.h workspace.h dwl.h client.h include/config.h
output.o: output.c output/output.h layers.h tree.h workspace.h dwl.h client.h include/config.h
tree.o: tree.c tree/tree.h dwl.h client.h layout.h util.h
workspace.o: workspace.c tree/workspace.h tree/tree.h dwl.h client.h layout.h util.h

# wayland-scanner is a tool which generates C headers and rigging for Wayland
# protocols, which are specified in XML. wlroots requires you to rig these up
# to your build system yourself and provide them in the include path.
WAYLAND_SCANNER   = `$(PKG_CONFIG) --variable=wayland_scanner wayland-scanner`
WAYLAND_PROTOCOLS = `$(PKG_CONFIG) --variable=pkgdatadir wayland-protocols`

include/cursor-shape-v1-protocol.h:
	$(WAYLAND_SCANNER) enum-header \
		$(WAYLAND_PROTOCOLS)/staging/cursor-shape/cursor-shape-v1.xml $@
include/pointer-constraints-unstable-v1-protocol.h:
	$(WAYLAND_SCANNER) enum-header \
		$(WAYLAND_PROTOCOLS)/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml $@
include/wlr-layer-shell-unstable-v1-protocol.h:
	$(WAYLAND_SCANNER) enum-header \
		protocols/wlr-layer-shell-unstable-v1.xml $@
include/wlr-output-power-management-unstable-v1-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		protocols/wlr-output-power-management-unstable-v1.xml $@
include/xdg-shell-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

include/config.h:
	cp include/config.def.h $@

apply:
	nix run .#apply

clean:
	rm -f dwl tree-viewer *.o include/*-protocol.h

dist: clean
	mkdir -p dwl-$(VERSION)
	cp -R LICENSE* Makefile CHANGELOG.md README.md include src \
		config.mk protocols dwl.1 dwl.desktop \
		dwl-$(VERSION)
	tar -caf dwl-$(VERSION).tar.gz dwl-$(VERSION)
	rm -rf dwl-$(VERSION)

install: dwl
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	rm -f $(DESTDIR)$(PREFIX)/bin/dwl
	cp -f dwl $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/dwl
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	cp -f dwl.1 $(DESTDIR)$(MANDIR)/man1
	chmod 644 $(DESTDIR)$(MANDIR)/man1/dwl.1
	mkdir -p $(DESTDIR)$(DATADIR)/wayland-sessions
	cp -f dwl.desktop $(DESTDIR)$(DATADIR)/wayland-sessions/dwl.desktop
	chmod 644 $(DESTDIR)$(DATADIR)/wayland-sessions/dwl.desktop

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/dwl $(DESTDIR)$(MANDIR)/man1/dwl.1 \
		$(DESTDIR)$(DATADIR)/wayland-sessions/dwl.desktop

.c.o:
	$(CC) $(CPPFLAGS) $(DWLCFLAGS) -o $@ -c $<

