# xdccget
This is a project that allows you to download files from IRC with XDCC with an easy and simple to use command line tool
like wget or curl. It supports at the moment Linux, Windows (with cygwin) and BSD-variants. Also OSX with some installed
ports works.

*Note*: This is an version of xdccget that runs on Windows 7 / 8 / 10 and 11 natively without the need for cygwin dlls. You can 
compile it for yourself with the free Visual Studio 2019 Community edition or you can just download it [here](https://github.com/TheBurningDust/xdccget-for-windows/releases/tag/1.3) from Github as a precompiled
binary executable for Windows x64 cpu architecture. You can just run the exe file, the openssl depencency is linked statically. Please
read the docs below for more infos on the program itself.

## Quick facts
* it is free software licenced under the GPL
* minimal usage of cpu and memory
* runs under Linux, BSDs, MacOSX and Windows (see notes for compiling below)
* support for IPv4 and IPv6 connections
* can be configured with configuration-file
* supports connection with and without SSL/TLS
* connection to passive bots are supported (you need to set the listen-ip and listen-port in this case, see comments below for more details)
* bots with support for ssend-command are supported

## Using xdccget
In order to use xdccget properly i will provide some simple examples. You should be able to extract 
the options for your personal usage quite quickly, i guess:

For example, let's assume that you want to download the package *34* from the bot *super-duper-bot*
at the channel *best-channel* from the irc-server
*irc.sampel.net* without ssl and from the standard-irc-port *6667*. 
Then the command line argument for xdccget would be:

``` 
xdccget -i "irc.sampel.net" "#best-channel" "super-duper-bot xdcc send #34"
``` 

This would download the package *34* from *super-duper-bot* without using ssl. You can also specifiy a 
special port, so lets assume that the *irc.sampel.net* server would use the port 1337. Then our xdcc-get-call would
be like this:

``` 
xdccget -i -p 1337 "irc.sampel.net" "#best-channel" "super-duper-bot xdcc send #34"
``` 

If your irc-network supports ssl you can even use an secure ssl-connection with xdccget. So lets imagine that 
*irc.sampel.net* uses ssl on port 1338. Then we would call xdccget like this to use ssl:

``` 
xdccget -i -p 1338 "#irc.sampel.net" "#best-channel" "super-duper-bot xdcc send #34"
``` 

Notice the #-character in front of irc.sampel.net. This tells xdccget to use ssl/tls on the connection to the irc-server.
If the bot even supports ssl than you can use the ssend-command to use an ssl-encrypted connection with the bot.
So for example if the *super-duper-bot* would support ssl-connection, then we could call xdccget like:

``` 
xdccget -i -p 1338 "#irc.sampel.net" "#best-channel" "super-duper-bot xdcc ssend #34"
``` 

Notice the *xdcc ssend* command instead of *xdcc send*. This tells the bot that we want connect to him with ssl 
enabled.

You can also join multiple channels, so if you also have to join #best-chat-channel in order to download packages from #best-channel, then you can call xdccget like:

``` 
xdccget -i "irc.sampel.net" "#best-channel, #best-chat-channel" "super-duper-bot xdcc send #34"
``` 

If the bot only supports the passive dcc mode, then you have to supply the listen ip (normally your public ip address) and the listen port (normally needs to be forwarded in your router). You can set the listen ip and port in the config file globally or you can set it with an command line argument. this is an example, where the public ip address is 2.2.2.2 and the port is 44444. the delay option in this command delays the sending of the send command by 70 seconds  and can be used, where you need to stay a certain amount of time in a channel.

``` 
xdccget -i --listen-ip=2.2.2.2 --listen-port=44444 --delay=70 "irc.sampel.net" "#best-channel" "super-duper-bot xdcc send #34"
``` 

If this does not work for you, please make sure, that the port is not blocked in your router and in your firewall. Normally all incoming connections are blocked, therefore you have to manually enable your chosen port.

This is the basic usage of xdccget. You can call xdccget --help to understand all currently supported arguments.
xdccget also uses a config file, which will be placed at your homefolder in .xdccget/config. You can modify
the default parameters to your matters quickly.

## Compiling xdccget
Compiling xdccget is just running make from the root folder of the repository. Please make sure, that you have installed
the depended libraries (OpenSSL and for some systems argp-library) and use the correct Makefile for your system.

### Ubuntu and derivants
To compile xdccget under Ubuntu and other distros like Linux Mint you have to install the package libssl-dev with apt-get.
You also need the build-essential package. 

```
sudo apt-get install libssl-dev build-essential
```

### other linux distros
You need to make sure, that you have the openssl-development packages for you favorite distribution installed.

### OSX and BSD
For osx and bsd systems you need to also install the development files for openssl. You need to install
the library argp, which is used to parse command line arguments. Please make sure, that you rename the Makefile.FreeBSD
for example to Makefile if you want to compile for FreeBSD.

If you use pkg on FreeBSD for package-management you can issue the following command to install the required libs:

```
sudo pkg install gcc argp-standalone openssl
```

On OSX and other BSD variants you have to use an alternative way to install the packages.

### Windows
For windows you first need to install cygwin. Please make sure that you install gcc-core, libargp and openssl-devel with
cygwin. If you have installed all depedent libraries then you can compile xdccget with cygwin by using the Makefile.cygwin.
Please rename Makefile.cygwin to Makefile and then run make from the cygwin terminal.

## Configure xdccget
You can configure some options with the config file. It is placed in the folder .xdccget in your home directory of your operating system. The following options are currently supported:

``` 
downloadDir     - this defines the default directory used to store the downloaded files
logLevel        - this defines the default logging level. valid options are info, warn and error
allowAllCerts   - this options will allow silently all self signed certificates if set to true
verifyChecksums - this option will automatically wait after the download completed to verify checksums. 
                  please note that if set to true xdccget does not exit after the download finished and 
                  you have to manually exit xdccget.
```
