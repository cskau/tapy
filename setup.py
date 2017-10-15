from distutils.core import setup
from distutils.core import Extension

tapy = Extension(
    'tapy',
    libraries = ['jack'],
    sources=['tapy.c'])

setup(
    name='tapy',
    version='0.1',
    description='A JACK client for recording and playing back audio.',
    ext_modules=[tapy])
