import configuration
import glob
import json
import run_benchmarks
import os.path

def gather_data():
    files = glob.glob(os.path.join(run_benchmarks.RESULTS_DIR, '*.json'))
    data = []
    for filename in files:
        with open(filename) as f:
            data.append(json.load(f))
    return data

def get_stat(all_params, data, argspec, key, value, outstat):
    """For a certain argspec, key, and value, finds the given statistic for each
    of the tables available

    """
    ret = {
        'x_axis': 'Table Type',
        'xs': [],
        'ys': [],
        'y_axis': ''
    }
    # We consider it a match if the configuration argspec is a subset of the
    # datum's argspec. So we must build a set of flag-value pairs.
    def build_argset(argsline):
        components = argsline.split()
        argset = set()
        for i in range(0, len(components), 2):
            argset.add((components[i], components[i + 1]))
        return argset
    argspec_set = build_argset(all_params.args[argspec])
    for datum in data:
        datum_argset = build_argset(datum['args'])
        if (argspec_set.issubset(datum_argset) and
            datum['key'] == key and
            datum['value'] == value):
            outstat_dict = datum['output'][outstat]
            ret['xs'].append(datum['table'])
            ret['ys'].append(outstat_dict['value'])
            ret['y_axis'] = '%s (%s)' % (outstat_dict['name'], outstat_dict['units'])
    return ret
