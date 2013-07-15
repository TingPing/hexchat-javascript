CFLAGS := -O2 -std=c++0x -Wall -fPIC -shared $(CFLAGS) 
CFLAGS += `pkg-config --libs --cflags mozjs185`
OUTFILE := javascript.so

all:
	cpp $(CFLAGS) javascript.cpp -o $(OUTFILE)

clean:
	rm $(OUTFILE)

install:
	install -m 644 -D $(OUTFILE) $(HOME)/.config/hexchat/addons/$(OUTFILE)

uninstall:
	rm $(HOME)/.config/hexchat/addons/$(OUTFILE)

