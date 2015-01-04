#!/usr/bin/env python

from setuptools import setup, Extension
version='0.3'
setup(name='camshr',
      version=version,
      description='Python support for Oak Ridge Camshr calls',
      long_description = """
      This module provides a limitted callable interface for the CAMSHR library
      """,
      author='Tom Fredian,Josh Stillerman',
      author_email='jas@www.mdsplus.org',
      url='http://www.mdsplus.org/',
#      download_url = 'http://www.mdsplus.org/binaries/python/',
      package_dir = {'camshr':'.',},
      include_package_data = False,
      packages = ['camshr',],
#      package_data = {'MDSplus':'*'},
      platforms = ('Any',),
      classifiers = [ 'Development Status :: 4 - Beta',
      'Programming Language :: Python',
      'Intended Audience :: Science/Research',
      'Environment :: Console',
      'Topic :: Scientific/Engineering',
      ],
      keywords = ('physics','mdsplus','camac',),
#       install_requires=['numpy','ctypes'],
#      include_package_data = True,
#      test_suite='tests.test_all',
#     install_requires=["pexpect","PyXML"],
      zip_safe = False,
     )
