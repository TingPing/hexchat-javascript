HexChat Javascript
==================

Javascript interface using Spidermonkey.

It's my first time using javascript and c++ so forgive my mistakes ;)

Consider this an alpha, everything works well enough to do useful things with it
but the api is very likely to change and some bugs still exist.


Downloads
---------

[Fedora x64](http://dl.tingping.se/fedora/x86_64/hexchat-javascript-0.1-1.fc19.x86_64.rpm)

[Windows x86](http://dl.tingping.se/hexchat-javascript/win32/hexchat-javascript.dll)


Installation
------------

###Linux:

If not on Fedora you can simply install the js-devel package from your distro of choice and *make && make install*.

###Windows:


Place the dll above in the *addons* folder within your [config dir](http://docs.hexchat.org/en/latest/settings.html#config-files)
and you must download the [xulrunner-sdk](https://ftp.mozilla.org/pub/mozilla.org/xulrunner/releases/10.0.4esr/sdk/xulrunner-10.0.4esr.en-US.win32.sdk.zip)
and add its *bin* folder to your PATH.


Features
--------

Done:

- Load/unload/reload/autoloading scripts
- Interpreter via */js*
- Runtime per script
- Full coverage of hexchat api

Todo:

- Error checking, testing, etc
- Objectify all the things?
- Improve build system
- Windows project
