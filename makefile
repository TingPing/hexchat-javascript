CXX ?= g++
PKG_CONFIG ?= pkg-config

CXXFLAGS ?= -O2
CXXFLAGS += -std=c++0x -fPIC \
			-Wall -Wextra -pedantic \
			-Wformat \
			-Wstrict-overflow=5 \
			-Wunused-macros \
			-Wno-unused-parameter \
			-Wno-write-strings

CXXFLAGS += $(shell $(PKG_CONFIG) --cflags mozjs185) \
			$(shell $(PKG_CONFIG) --cflags hexchat-plugin)
LDFLAGS += -shared
LIBS += $(shell $(PKG_CONFIG) --libs mozjs185)

PREFIX ?= /usr
OUTFILE := javascript.so

INSTALLDIR := $(DESTDIR)$(shell $(PKG_CONFIG) --variable=hexchatlibdir hexchat-plugin)
DATADIR := $(DESTDIR)$(PREFIX)/share

all:
	$(CXX) $(CXXFLAGS) $(LDFLAGS) javascript.cpp -o $(OUTFILE) $(LIBS)

clean:
	rm -f $(OUTFILE)

install:
	install -m644 -D $(OUTFILE) "$(INSTALLDIR)/$(OUTFILE)"
	install -m644 "data/hexchat-javascript.metainfo.xml" -t "$(DATADIR)/appdata"

uninstall:
	rm -f "$(INSTALLDIR)/$(OUTFILE)"
	rm -f "$(DATADIR)/appdata/hexchat-javascript.metainfo.xml"

