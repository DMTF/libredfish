# Copyright Notice:
# Copyright 2017-2019 DMTF. All rights reserved.
# License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/main/LICENSE.md
from distutils.core import setup, Extension

module1 = Extension('libredfish',
                    define_macros = [('NO_CZMQ', 1)],
                    include_dirs = ['include'],
                    libraries = ['jansson', 'curl'],
                    sources = ['src/main.c', 'src/payload.c', 'src/redpath.c', 'src/service.c', 'src/asyncEvent.c', 'src/asyncRaw.c', 'src/queue.c', 'src/util.c', 'bindings/python/pyredfish.c'])

setup (name = 'libRedfish',
       version = '1.0',
       ext_modules = [module1])
