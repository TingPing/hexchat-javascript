HexChat Javascript
==================

[![Build Status](https://travis-ci.org/TingPing/hexchat-javascript.png?branch=master)](https://travis-ci.org/TingPing/hexchat-javascript)
[![Coverity Badge](https://scan.coverity.com/projects/1399/badge.svg)](https://scan.coverity.com/projects/1399)

Javascript interface using Spidermonkey.

This plugin is no longer actively developed though bugs may be fixed.  
I suggest using the Lua plugin shipping with modern HexChat as an alternative.

For more information check out the [wiki](https://github.com/TingPing/hexchat-javascript/wiki).


Features
--------

- Load/unload/reload/autoloading scripts
- Interpreter via */js*
- Runtime per script
- Full coverage of hexchat api
- Windows and Unix support

Requirements
------------

- HexChat 2.9.6+
- JS 1.8.5

Installation
------------

###Windows:

1. Place the [javascript.dll](https://github.com/TingPing/hexchat-javascript/releases) in the *addons* folder within your [config dir](http://docs.hexchat.org/en/latest/settings.html#config-files).
2. Install Javascript, instructions are on the [wiki](https://github.com/TingPing/hexchat-javascript/wiki/Javascript-on-Windows).

###Fedora:

1. Download a repo file from [here](http://copr-fe.cloud.fedoraproject.org/coprs/tingping/hexchat-javascript/) into */etc/yum.repos.d/*
2. *yum install hexchat-javascript*

###Arch:

1. Download package from [hexchat-javascript-git](https://aur.archlinux.org/packages/hexchat-javascript-git/) on the AUR.
2. Follow the [wiki](https://wiki.archlinux.org/index.php/Arch_User_Repository#Installing_packages) to install.

Building
--------

###Linux:

1. Install the hexchat and js dev package from your distro of choice. (e.g. js-devel, libmozjs185-dev, js185)
2. *make && sudo make install*

###Windows:

1. Install my build of Javascript as mentioned on the [wiki](https://github.com/TingPing/hexchat-javascript/wiki/Javascript-on-Windows) and update its path in [hexchat-javascript.props](win32/hexchat-javascript.props).
2. Open in [Visual Studio](https://www.microsoft.com/visualstudio/eng/downloads#d-express-windows-desktop) and build.
