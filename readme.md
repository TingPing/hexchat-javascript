HexChat Javascript
==================

[![Build Status](https://travis-ci.org/TingPing/hexchat-javascript.png?branch=master)](https://travis-ci.org/TingPing/hexchat-javascript)

Javascript interface using Spidermonkey.

Consider this an beta, everything works well enough to do useful things with it
but the api is very likely to change and some bugs still exist.

For more information check out the [wiki](https://github.com/TingPing/hexchat-javascript/wiki).


Features
--------

- Load/unload/reload/autoloading scripts
- Interpreter via */js*
- Runtime per script
- Full coverage of hexchat api
- Windows and Unix support


Downloads
---------

- [Fedora](http://dl.tingping.se/fedora/x86_64/)
- [Arch](https://aur.archlinux.org/packages/hexchat-javascript-git/)
- [Windows](https://github.com/TingPing/hexchat-javascript/releases)


Requirements
------------

- HexChat 2.9.6+
- JS 1.8.5

Installation
------------

###Windows:

1. Place the javascript.dll linked above in the *addons* folder within your [config dir](http://docs.hexchat.org/en/latest/settings.html#config-files).
2. Install Javascript, instructions are on the [wiki](https://github.com/TingPing/hexchat-javascript/wiki/Javascript-on-Windows).


Building
--------

###Linux:

1. Install the hexchat and js dev package from your distro of choice.
2. *make && make install*

###Windows:

1. Install my build of Javascript as mentioned on the [wiki](https://github.com/TingPing/hexchat-javascript/wiki/Javascript-on-Windows) and update its path in [hexchat-javascript.props](win32/hexchat-javascript.props).
2. Open in [Visual Studio](https://www.microsoft.com/visualstudio/eng/downloads#d-express-windows-desktop) and build.
