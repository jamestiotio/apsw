Installation and customization
==============================

.. currentmodule:: apsw

.. _pypi:

PyPI (recommended)
------------------

APSW is on PyPI at https://pypi.org/project/apsw/

It can be installed in the same way as other packages::

    python3 -m pip install apsw

When you install from PyPI:

* The corresponding SQLite version is embedded privately inside
  and not affected by or visible to the rest of the machine or
  even the rest of the process.

  This means other modules and libraries will continue using
  whatever SQLite they would have before.  For example `Core Data
  <https://developer.apple.com/documentation/coredata>`__ on MacOS
  uses SQLite, but will not know of or be affected by the SQLite
  inside APSW.

* All :doc:`extensions <extensions>` are enabled, except ICU.

* `SQLITE_ENABLE_COLUMN_METADATA <https://www.sqlite.org/compile.html#enable_column_metadata>`__
  is enabled, providing :attr:`Cursor.description_full`

The PyPI releases include pre-built binaries for common platforms.  If yours is not covered, then
pip will download the source release and automatically compile with the same settings.  It will
require a C compiler and the Python development header files.


Linux/BSD provided
------------------

Most Linux & BSD distributions have packaged APSW which may trail the
SQLite and APSW releases by a year, or more. The distribution provided
APSW uses the system wide SQLite library.

.. list-table::
    :widths: auto

    * - Debian
      - Install `python3-apsw <https://packages.debian.org/python3-apsw>`__
    * - Fedora
      - Install `python3-apsw <https://packages.fedoraproject.org/pkgs/python-apsw/>`__
    * - Ubuntu
      - Install `python3-apsw <https://packages.ubuntu.com/search?suite=all&searchon=names&keywords=apsw>`__
    * - Gentoo
      - Install `dev-python/apsw <https://packages.gentoo.org/package/dev-python/apsw>`_
    * - Arch
      - Install `python-apsw <https://www.archlinux.org/packages/?q=apsw>`__
    * - FreeBSD
      - `databases/py-apsw <https://cgit.freebsd.org/ports/tree/databases/py-apsw>`__ in `Ports <https://docs.freebsd.org/en/books/handbook/ports/>`__

There is a `full list (150+)
<https://repology.org/project/python:apsw/versions>`__ of
distributions, the package name for APSW, and what APSW version they
are currently on.

Source
------

It is recommended you get the source from `Github releases
<https://github.com/rogerbinns/apsw/releases>`__.  If you get the
source from `PyPi <https://pypi.org/project/apsw/>`__ then ensure you
edit the :file:`setup.apsw` file inside.

.. downloads-begin

* `apsw-3.44.2.0.zip
  <https://github.com/rogerbinns/apsw/releases/download/3.44.2.0/apsw-3.44.2.0.zip>`__
  (Source, includes this HTML Help)

* `apsw-3.44.2.0-sigs.zip 
  <https://github.com/rogerbinns/apsw/releases/download/3.44.2.0/apsw-3.44.2.0-sigs.zip>`__
  GPG signatures for all files

.. downloads-end


.. _verifydownload:

Verifying your download
^^^^^^^^^^^^^^^^^^^^^^^

Github `source releases <https://github.com/rogerbinns/apsw/releases>`__ are
digitally signed so you can verify they have not been tampered with.
Download and extract the corresponding zip file of signatures.  These
instructions are for `GNU Privacy Guard <https://www.gnupg.org/>`__.
(GPG is installed as standard on most Unix/Linux platforms and can be
downloaded for Windows.)

Verify

  To verify a file use --verify specifying the corresponding
  ``.asc`` filename.  This example verifies the source:

  .. code-block:: console

      $ gpg --verify apsw-3.44.2.0.zip.asc
      gpg: Signature made ... date ... using DSA key ID 0DFBD904
      gpg: Good signature from "Roger Binns <rogerb@rogerbinns.com>"

  If you get a "good signature" then the file has not been tampered with
  and you are good to go.

Getting the signing key

  You may not have the signing key available in which case the last
  line will be something like this:

  .. code-block:: output

     gpg: Can't check signature: public key not found

  You can get a copy of the key using this command:

  .. code-block:: console

    $ gpg --keyserver hkp://keyserver.ubuntu.com --recv-keys 0DFBD904
    gpg: requesting key 0DFBD904 from hkp server keyserver.ubuntu.com
    gpg: /home/username/.gnupg/trustdb.gpg: trustdb created
    gpg: key 0DFBD904: public key "Roger Binns <rogerb@rogerbinns.com>" imported
    gpg: Total number processed: 1
    gpg:               imported: 1

  Repeat the verify step.


.. _build:

Building and customization
--------------------------

APSW is configured for standard building (:pep:`518`)

.. code-block:: console

    $ python3 -m build

You will need to update the MANIFEST first if you are providing your
own SQLite, or if you are providing a ``setup.apsw`` with custom
configuration. `setuptools
<https://setuptools.pypa.io/en/latest/index.html>`__ is used to
compile the extension.  You can use it directly instead by invoking
``setup.py``.

Build process
^^^^^^^^^^^^^

A series of commands and options are given to :file:`setup.py` in this pattern:

.. code-block:: shell

    python setup.py cmdone --option --option value cmdtwo --option \
       cmdthree --option --option value

The only necessary command is **build**.  You can get help by `--help`:

.. code-block:: shell

    python setup.py build --help

Each command takes options which can be specified on the command line,
or in a configuration file named :file:`setup.cfg` or
:file:`setup.apsw`.  The leading double dash on options is omitted,
and dashes inside should become underscores.

.. literalinclude:: ../tools/setup-pypi.cfg
   :language: ini

SQLite options
^^^^^^^^^^^^^^

It is important to understand `SQLite's compile time options
<https://www.sqlite.org/compile.html>`__.  They provide control
over functionality and APIs included or excluded from SQLite.

APSW needs to know the options chosen so it can adapt.  For example if
extension loading is omitted from SQLite then APSW also needs to omit
the same functionality, otherwise compilation or linking will fail.

Finding SQLite
^^^^^^^^^^^^^^

APSW can fetch SQLite as detailed below, and places it in a
:file:`sqlite3/` subdirectory.  You can place your own SQLite in that
directory.  If there is a :file:`sqlite3.c` (ie the `amalgamation
<https://www.sqlite.org/amalgamation.html>`__) then it will be
statically included inside APSW.  A compiled SQLite will be picked up
if present.  If none of that is present, then the standard compiler
locations are used (eg :file:`/usr/include` on Unix).

If :file:`sqlite3/sqlite3config.h` is present it is included before
:file:`sqlite3/sqlite3.c`.  It is a good location to put `platform
configuration
<https://www.sqlite.org/compile.html#_platform_configuration>`__ which
APSW's fetch does automatically by running :file:`configure`.


.. _setup_py_flags:

setup.py commands and their options
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

These are the relevant :file:`setup.py` commands and their relevant options.

.. _setup_build_flags:

build
#####

Does the complete build.  This will invoke `build_ext` - use only one of
`build` or `build_ext`.

.. list-table::
    :widths: auto

    * - ``--fetch``
      - Fetches the corresponding SQLite version
    * - ``--enable-all-extensions``
      - Enables all the :doc:`standard extensions <extensions>`
    * - ``--enable``
      - A comma separated list of `options to enable that are normally
        off
        <https://www.sqlite.org/compile.html#_options_to_enable_features_normally_turned_off>`__
        omitting the :code:`SQLITE_ENABLE` prefix.  They will be
        uppercased.  eg :code:`--enable column_metadata,fts5`
    * - ``--omit``
      - A comma separated list of `options to omit that are normally
        enabled <https://www.sqlite.org/compile.html#_options_to_omit_features>`__
        omitting the :code:`SQLITE_OMIT` prefix.  They will be
        uppercased.  eg :code:`--omit automatic_index`


.. _fetch_checksums:

fetch
#####

This provides more fine grained control over what is fetched.

..  list-table::
    :widths: auto

    * - ``--version``
      - Specify an explicit version of SQLite to fetch
    * - ``--fetch-sqlite``
      - Downloads the SQLite amalgamation
    * - ``--all``
      - Downloads all SQLite components other than the amalgamation.
        Over time this has included additional extensions and SQLite
        functions, but currently is nothing.
    * - ``--missing-checksum-ok``
      - APSW includes checksums of SQLite releases and will fail a
        fetch if you specify a version for which no checksum is known.
        This allows proceeding.


.. _matching_sqlite_options:

build_ext
#########

This performs the compilation of the C code, and provides more control than build.

..  list-table::
    :widths: auto

    * - ``--use-system-sqlite-config``
      - Uses :mod:`ctypes` to determine the system wide SQLite library
        compilation options
    * - ``--definevalues``
      - Additional #defines separated by commas.  eg :code:`--definevalues
        SQLITE_MAX_ATTACHED=37,SQLITE_EXTRA_INIT=mycore_init`
    * - ``--enable-all-extensions``
      - Enables all the :doc:`standard extensions <extensions>`
    * - ``--enable``
      - A comma separated list of `options to enable that are normally
        off
        <https://www.sqlite.org/compile.html#_options_to_enable_features_normally_turned_off>`__
        omitting the :code:`SQLITE_ENABLE` prefix.  They will be
        uppercased.  eg :code:`--enable column_metadata,fts5`
    * - ``--omit``
      - A comma separated list of `options to omit that are normally
        enabled <https://www.sqlite.org/compile.html#_options_to_omit_features>`__
        omitting the :code:`SQLITE_OMIT` prefix.  They will be
        uppercased.  eg :code:`--omit automatic_index`
    * - ``--apsw-no-old-names``
      - Excludes old non :pep:`8` :ref:`complaint name aliases
        <renaming>` from the extension and type stubs.

.. _testing:

Testing
-------

SQLite itself is `extensively tested
<https://sqlite.org/testing.html>`__. It has considerably more code
dedicated to testing than makes up the actual database functionality.

APSW includes tests which use the standard Python testing modules to
verify correct operation. New code is developed alongside the tests.
Reported issues also have test cases to ensure the issue doesn't
happen or doesn't happen again.:

.. code-block:: output

                  Python  /usr/bin/python3 sys.version_info(major=3, minor=11, micro=6, releaselevel='final', serial=0) 64bit ELF
  Testing with APSW file  /space/apsw/apsw/__init__.cpython-311-x86_64-linux-gnu.so
            APSW version  3.44.0.0
      SQLite lib version  3.44.0
  SQLite headers version  3044000
      Using amalgamation  True
  .............................................................A message due to RecursionError is possible, and what is being tested
  object address  : 0x7fea3c94ada0
  object refcount : 1
  object type     : 0x9de7a0
  object type name: TypeError
  object repr     :
  lost sys.stderr
  object address  : 0x7fea3c94ad40
  object refcount : 1
  object type     : 0x9de7a0
  object type name: TypeError
  object repr     :
  lost sys.stderr
  ................................................................
  ----------------------------------------------------------------------
  Ran 125 tests in 26.783s

  OK

The tests also ensure that as much APSW code as possible is executed
including alternate paths through the code.  95.5% of the APSW code is
executed by the tests. In the source, there is a script that enables
extra code that deliberately induces extra conditions such as memory
allocation failures, SQLite returning error codes, Python APIs
erroring etc.  That brings coverage up to 99.6% of the code.

A memory checker `Valgrind <https://valgrind.org>`_  and  `compiler
sanitizer options <https://en.wikipedia.org/wiki/AddressSanitizer>`__
are also used for further validation.

To ensure compatibility with the various Python versions, a script
downloads and compiles all supported Python versions in both debug and
release configurations (and 32 and 64 bit) against the APSW and SQLite
supported versions running the tests.

In short both SQLite and APSW have a lot of testing!