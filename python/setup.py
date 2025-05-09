#!/usr/bin/env python

from setuptools import setup, find_packages

setup(name='uhdm',
      version='0.1',
      description='UHDM VPI SWIG wrapper and utilities',
      url='https://github.com/ChipsAlliance/UHDM',
      packages=find_packages(),
      install_requires=['orderedmultidict'],
      package_data={'uhdm': ['_py_uhdm.so']},
      include_package_data=True,
)

