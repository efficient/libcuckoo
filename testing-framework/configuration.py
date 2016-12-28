import json
from collections import namedtuple
import itertools

class AllParameters(object):
    """AllParameters holds all the possible configuration parameters for a
    build"""

    def __init__(self):
        """Loads all the configuration files"""
        with open('arguments.json') as f:
            self.args = json.load(f)
        with open('keys.json') as f:
            self.keys = json.load(f)
        with open('values.json') as f:
            self.values = json.load(f)
        with open('tables.json') as f:
            self.tables = json.load(f)

    def get_build_configurations(self):
        """Yields a sequence of all possible build configurations"""
        for (key, value, table) in itertools.product(
                self.keys, self.values, self.tables):
            yield BuildConfiguration(key, value, table)

class BuildConfiguration(object):
    """A BuildConfiguration holds the parameters that uniquely identify a build,
    namely, the key, value, and table type."""

    def __init__(self, key, value, table):
        self.key = key
        self.value = value
        self.table = table

    def __str__(self):
        return 'BuildConfiguration("%s", "%s", "%s")' % (
            self.key, self.value, self.table)

    def __repr__(self):
        return str(self)

    def cmake_parameters(self):
        """Returns a list of parameters to CMake for building this
        configuration"""
        return [
            '-DBUILD_UNIVERSAL_BENCHMARK=1',
            '-DUNIVERSAL_KEY=%s' % self.key,
            '-DUNIVERSAL_VALUE=%s' % self.value,
            '-DUNIVERSAL_TABLE=%s' % self.table
        ]

    def build_dir(self):
        """Returns a build directory unique to the configuration"""
        return 'build___%s___%s___%s' % (self.key, self.value, self.table)

    def result_file(self, argspec):
        return 'results___%s___%s___%s___%s.json' % (
            self.key, self.value, self.table, argspec)
