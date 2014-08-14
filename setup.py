import os
import subprocess

from setuptools import setup
from distutils.command.build import build

class YBinlogPBuild(build):
    def initialize_options(self):
        build.initialize_options(self)
        self.build_base = os.path.join(self.build_base, 'python')

    def run(self):
        subprocess.call(['make', 'build'])
        build.run(self)

about = {}
with open(os.path.join(os.path.dirname(__file__), 'src/ybinlogp/version.py')) as f:
    exec f.read() in about

setup(
    author='Yelp',
    author_email='yelplabs@yelp.com',
    cmdclass={'build': YBinlogPBuild},
    data_files=[('lib', ['build/libybinlogp.so', 'build/libybinlogp.so.1']),
                ('include', ['src/ybinlogp.h'])],
    description='Library, program, and python bindings for parsing MySQL binlogs',
    license='BSD',
    name='YBinlogP',
    package_dir={'': 'src'},
    packages=['ybinlogp'],
    scripts=['build/ybinlogp'],
    url='http://github.com/Yelp/ybinlogp',
    version=about['__version__']
)
