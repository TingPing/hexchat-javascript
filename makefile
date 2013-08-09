CXX ?= g++
PKG_CONFIG ?= pkg-config

CXXFLAGS ?= -O2
CXXFLAGS += -std=c++0x -Wall -fPIC
CXXFLAGS += $(shell $(PKG_CONFIG) --cflags mozjs185) \
			$(shell $(PKG_CONFIG) --cflags hexchat-plugin)
LDFLAGS += -shared
LIBS += $(shell $(PKG_CONFIG) --libs mozjs185)
OUTFILE := javascript.so

all:
	$(CXX) $(CXXFLAGS) $(LDFLAGS) javascript.cpp -o $(OUTFILE) $(LIBS)

clean:
	rm $(OUTFILE)

install:
	install -m 644 -D $(OUTFILE) "$(DESTDIR)$(shell $(PKG_CONFIG) --variable=hexchatlibdir hexchat-plugin)/$(OUTFILE)"

uninstall:
	rm "$(DESTDIR)$(shell $(PKG_CONFIG) --variable=hexchatlibdir hexchat-plugin 2>/dev/null)/$(OUTFILE)"

