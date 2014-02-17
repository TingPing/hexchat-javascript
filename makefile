CXX ?= g++
PKG_CONFIG ?= pkg-config

CXXFLAGS ?= -O2
CXXFLAGS += -std=c++0x -fPIC \
			-Wall -Wextra -pedantic \
			-Wformat \
			-Wstrict-overflow=5 \
			-Wunused-macros \
			-Wno-unused-parameter

CXXFLAGS += $(shell $(PKG_CONFIG) --cflags mozjs185) \
			$(shell $(PKG_CONFIG) --cflags hexchat-plugin)
LDFLAGS += -shared
LIBS += $(shell $(PKG_CONFIG) --libs mozjs185)
OUTFILE := javascript.so
INSTALLDIR := $(DESTDIR)$(shell $(PKG_CONFIG) --variable=hexchatlibdir hexchat-plugin)

all:
	$(CXX) $(CXXFLAGS) $(LDFLAGS) javascript.cpp -o $(OUTFILE) $(LIBS)

clean:
	rm $(OUTFILE)

install:
	install -m644 -D $(OUTFILE) "$(INSTALLDIR)/$(OUTFILE)"

uninstall:
	rm -f "$(INSTALLDIR)/$(OUTFILE)"
