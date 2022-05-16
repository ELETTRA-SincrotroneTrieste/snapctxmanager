INSTALLATION INSTRUCTIONS FOR the Hdbextractor++ library

=============================== PART 1 =======================================
|                                                                            |
================ INSTRUCTIONS FOR THE END USER ===============================

1. Building the hdbextractor core library

1a. Dependencies: mysql and mysql development files

1b. Build the library:

    $ cd hdbextractor/trunk

    $ ./configure --prefix=/usr/local/hdbextractor++  (adjust prefix as you like)

    $ make

    $ make install

    The documentation is not built by default.

1c. Build the documentation
    By default, html documentation is built using doxygen.
    If you want to build also a pdf documentation, run ./configure with the
    --enable-doxygen-pdf option.

    Other doxygen related options are available through ./configure --help

    To build the documentation (doxygen is needed):

    $ make doc

    To install the html documentation:

    $ make install-html

    pdf  documentation is generated inside: doc/hdbextractor.pdf
    html documentation is generated inside: doc/html

    NOTE: only html documentation is installed by `make install-html`

=============================== PART 2 =======================================
|                                                                            |
================ INSTRUCTIONS FOR THE DEVELOPER ==============================

    $ cd hdbextractor/trunk

    $ aclocal
    $ autoconf # to generate the configure script on the base of the configure.ac file
    $ automake --add-missing
    $ autoconf

    Then 1a, 1b, 1c

    NOTES:

    * AC_PREREQ([2.60]) in configure.ac defines the minimum autoconf version required.
    * To prepare a distributable version of the library, run

      $ make dist

      This will setup all the necessary files (e.g. placing the config.sub, config.guess
      correct files in the tarball) and will produce a tar.gz inside the working directory.

    * $ make distdir

      will make a directory with the files to distribute (without compression)

   POSSIBLE PROBLEMS:

   *   Some autotools require AC_CONFIG_MACRO_DIR instead of AC_CONFIG_MACRO_DIRS:
       AC_CONFIG_MACRO_DIR([m4])
       # AC_CONFIG_MACRO_DIRS([m4])

   *   configure can detect Python installation but make can't find Python.h include file:
       - open pyext/Makefile.am and add  -I/usr/include/python2.6 (or whatever) to INCLUDEDIRS directive
