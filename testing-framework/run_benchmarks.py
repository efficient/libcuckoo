import configuration
import logging
import os
import os.path
import shutil
import subprocess

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CMAKE_ROOT = os.path.join(SCRIPT_DIR, '..')
RESULTS_DIR = os.path.abspath('results')

def create_build_configuration(build_config, force_rebuild=False):
    """Creates the build specified by the build configuration"""
    build_dir = os.path.abspath(build_config.build_dir())
    if os.path.exists(build_dir):
        if force_rebuild:
            shutil.rmtree(build_dir)
        else:
            return
    logging.info('Creating build directory: %s' % build_dir)
    os.mkdir(build_dir)
    os.chdir(build_dir)
    if os.path.exists('CMakeCache.txt'):
        logging.info("Removing 'CMakeCache.txt'")
        os.remove('CMakeCache.txt')
    cmake_command = ['cmake'] + build_config.cmake_parameters() + [CMAKE_ROOT]
    logging.info('Running: %s' % ' '.join(cmake_command))
    subprocess.check_call(cmake_command)
    os.chdir(SCRIPT_DIR)

def run_configuration(all_params, build_config, argspecs):
    """Given the set of all parameters, a build configuration, and a set of
    argument specifications, run those specifications and pipe the results into
    the results directory"""
    build_dir = os.path.abspath(build_config.build_dir())
    os.chdir(build_dir)
    logging.info('Building universal_benchmark executable in directory: %s '
                 % build_dir)
    subprocess.check_call(['make', 'universal_benchmark'])
    os.chdir('..')
    if not os.path.exists(RESULTS_DIR):
        logging.info('Creating results directory: %s' % RESULTS_DIR)
        os.mkdir(RESULTS_DIR)

    for argspec in argspecs:
        cmdline_args = all_params.args[argspec]
        output_file = os.path.join(RESULTS_DIR,
                                   build_config.result_file(argspec))
        command = [os.path.join(build_dir, 'tests', 'universal-benchmark',
                                'universal_benchmark')] + cmdline_args.split()
        with open(output_file, 'w') as f:
            logging.info('Running: %s' % ' '.join(command))
            logging.info('Piping output to: %s' % output_file)
            subprocess.check_call(command, stdout=f, stderr=f)
    os.chdir(SCRIPT_DIR)

if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)
    all_params = configuration.AllParameters()
    for build_config in all_params.get_build_configurations():
        create_build_configuration(build_config)
        run_configuration(all_params, build_config, all_params.args.keys())
