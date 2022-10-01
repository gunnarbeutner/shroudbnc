# shroudBNC

shroudBNC is a modular IRC proxy written in C++. It is capable of proxying IRC connections for multiple users. Using TCL scripts it can be extended.

# Installation

First of all make sure that you have the following software (and accompanying development files) installed:

- C++ compiler (usually GCC)
- Autoconf
- Automake
- Libtool
- OpenSSL (if you're going to use SSL-encrypted connections; README.ssl contains more details about how to use SSL)
- SWIG and TCL (if you're going to use the tcl module, version 8.4 or later, earlier versions have not been tested)
- Optional: c-ares

On Debian you can install all the required packages using this command:

    apt install build-essential autoconf automake libtool libssl-dev tcl-dev swig libc-ares-dev

In order to compile and install shroudBNC you will have to use the `autogen.sh` and `configure` scripts:

    $ ./autogen.sh
    $ ./configure

If you are planning to install shroudBNC as root you will need to manually specify another prefix, e.g.:

    # ./configure --prefix=/usr/local

Note: While loading TCL scripts shroudBNC will fall back to trying the `$PREFIX/share/sbnc` directory when a script isn't available in the user's configuration directory. Therefore it is NOT necessary to use absolute paths in the `sbnc.tcl` configuration file.

The configure script will tell you which features have been enabled (e.g. SSL, TCL). In case any of the required libraries are missing you can now fix these errors and re-run the configure script.

Afterwards you can build shroudBNC:

    $ make
    $ make install

Unless you have manually specified another prefix `make install` will install shroudBNC in a directory named `sbnc` in your home directory:

    [gunnar@hermes ~]$ ls -l ~/sbnc
    total 6
    drwxr-xr-x  2 gunnar  gunnar    3 Aug 22 18:28 bin
    drwxr-xr-x  3 gunnar  gunnar    3 Aug 22 18:28 lib
    -rwxr-xr-x  1 gunnar  gunnar  243 Aug 22 18:28 sbnc
    drwxr-xr-x  3 gunnar  gunnar    3 Aug 22 18:28 share
    [gunnar@hermes ~]$

Once you've compiled shroudBNC you will need to edit its configuration files:

You can do that by simply starting shroudBNC in the installation directory:

    [gunnar@hermes ~]$ cd ~/sbnc
    [gunnar@hermes ~/sbnc]$ ./sbnc
    shroudBNC (version: 1.3alpha18 $Revision: 1158 $) - an object-oriented IRC bouncer
    Configuration directory: /usr/home/gunnar/sbnc
    [Sun August 22 2010 18:37:29]: Log system initialized.
    No valid configuration file has been found. A basic
    configuration file can be created for you automatically. Please
    answer the following questions:
    1. Which port should the bouncer listen on (valid ports are in the range 1025 - 65535): 9000
    2. What should the first user's name be? test
    Please note that passwords will not be echoed while you type them.
    1. Please enter a password for the first user:
    2. Please confirm your password by typing it again:
    Writing main configuration file... DONE
    Writing first user's configuration file... DONE
    Configuration has been successfully saved. Please restart shroudBNC now.
    [gunnar@hermes ~/sbnc]$

If you've installed shroudBNC as root you can simply start `sbnc` in any directory (as a non-root user). shroudBNC will pick up any existing configuration directories (i.e. `~/sbnc`, `~/.sbnc` or the current working directory). If no configuration directory could be found it defaults to `~/.sbnc` and asks you to create a config file.

After creating the configuration files you can start shroudBNC:

    [gunnar@hermes ~/sbnc]$ ./sbnc
    shroudBNC (version: 1.3alpha18 $Revision: 1158 $) - an object-oriented IRC bouncer
    Configuration directory: /usr/home/gunnar/sbnc
    [Sun August 22 2010 18:42:53]: Log system initialized.
    [Sun August 22 2010 18:42:53]: Created main listener.
    [Sun August 22 2010 18:42:53]: Starting main loop.
    Daemonizing... DONE
    [gunnar@hermes ~/sbnc]$

You might want to edit the sbnc.tcl config file if you plan to use the TCL module. Load any scripts you want. You will have to rehash the tcl module after every change you make to `sbnc.tcl` or any other `.tcl` script which you have loaded:

    /sbnc tcl rehash (while you're connected to the bouncer using an IRC client)

# Usage

Just connect to the bouncer using your favorite IRC client. You will have to set your username (i.e. e-mail address in most clients) to the account name you've chosen for your bouncer account. If for some reason you can't change your client's username you can specify the username in the password setting (of the form `username:password`).

Once you're connected you should type /msg -sBNC help to get a list of available commands.

shroudBNC supports oidentd in order to provide unique idents for each bouncer user. You will need to enable ident-spoofing for the Unix account you're using to run shroudBNC if you want each user to have their own ident. Read oidentd's manual.

# TCL

The tcl module is automatically built and installed if the configure script was able to find the appropriate tcl libraries and headers.

If the configure script was unable to find the TCL libraries/headers and you are sure that they are installed you might use the --with-tcl parameter to specify the location of the tclConfig.sh file manually; e.g.:

    ./configure --with-tcl=/usr/local/ActiveTcl/tcl8.4

# Log files

shroudBNC automatically re-creates log files if they disappear during runtime. It also re-opens them if their inode of dev numbers change. Therefore rotating logs is as easy as moving the existing logs to a different location.

# Security

It is vital that you understand that any bouncer admin has access to:

a) the shell account where the bouncer is running  
b) any user connections

Using the TCL module any admin can perform shell commands (using the exec command) and send IRC commands for any user's connection. Thus you should choose your admins wisely. Do not load arbitrary scripts and make sure you understand any scripts you want to load.

# Credits

I'd like to thank all the beta testers who have been using shroudBNC so far. Feature requests and bug reports are welcome; please use our issue tracker at https://github.com/gunnarbeutner/shroudbnc

In case you encounter problems while compiling and/or using shroudBNC you can contact us on IRC:

Server: irc.quakenet.org / 6667  
Channel: #sbnc

# Third Party Software

This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit (http://www.openssl.org/)

For a more detailed list of third-party code in this project please refer to the README.copyright file.
