CFLAGS := -O2 -std=c++0x -Wall -fPIC -shared
CFLAGS += `pkg-config --libs --cflags libjs`
OUTFILE := javascript.so

all:
	c++ $(CFLAGS) javascript.cpp -o $(OUTFILE)

clean:
	rm $(OUTFILE)

install:
	install -m 644 -D $(OUTFILE) $(HOME)/.config/hexchat/addons/$(OUTFILE)

uninstall:
	rm $(HOME)/.config/hexchat/addons/$(OUTFILE)

