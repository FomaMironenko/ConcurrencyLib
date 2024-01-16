#!/usr/bin/python3

import os
import sys
import shutil


def print_b(text):
    print(f'\033[94m{text}\033[0m')
    
def print_lb(text):
    print(f'\033[36m{text}\033[0m')
    
def print_r(text):
    print(f'\033[31m{text}\033[0m')
    
def print_g(text):
    print(f'\033[32m{text}\033[0m')



test_dir = 'tests'

builds = {
    'asan': {
        'type':     'Asan',
        'dir':      'build/asan',
    },
    'tsan': {
        'type':     'Tsan',
        'dir':      'build/tsan',
    },
    'ubsan': {
        'type':     'UBsan',
        'dir':      'build/ubsan',
    },
    'asan_ubsan': {
        'type':     'AsanWithUBsan',
        'dir':      'build/ub_asan',
    },
    'tsan_ubsan': {
        'type':     'TsanWithUBsan',
        'dir':      'build/ub_tsan',
    }
}


def run_one_build(cwd, build):
    # Go to project root
    os.chdir(cwd)
    
    # Print build message
    build_msg = f'Building tests with build type: ==== {build["type"]} ===='
    print_b('\n\n' + '=' * len(build_msg))
    print_b(build_msg)
    print_b('=' * len(build_msg) + '\n\n')
    
    # Cretae build directory
    assert len(build['dir']) != 0
    build_dir = os.path.join(cwd, build['dir'])
    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)
    os.makedirs(build_dir)
    os.chdir(build_dir)
    
    # Build project
    CMAKE_COMMAND = f'cmake -DCMAKE_BUILD_TYPE={build["type"]} {cwd}'
    ret = os.system(CMAKE_COMMAND)
    if ret != 0:
        raise Exception('ERROR: unexpected cmake error')
    MAKE_COMMAND = 'make -j20'
    ret = os.system(MAKE_COMMAND)
    if ret != 0:
        raise Exception('ERROR: unexpected build error')
    
    # Find test files
    test_path = os.path.join(build_dir, test_dir)
    executables = list(filter(lambda name: name.endswith('Test'), os.listdir(test_path)))
    
    # Print run message
    run_msg = f'Running {len(executables)} tests with build type: ==== {build["type"]} ===='
    print_b('\n' + '=' * len(run_msg))
    print_b(run_msg)
    print_b('=' * len(run_msg) + '\n')
    
    # Run tests
    num_failed = 0
    num_success = 0
    failed = []
    for idx, file in enumerate(executables):
        print_lb(f'#### {idx + 1 : 2d}  Running test "{file}" under {build["type"]}')
        RUN_COMMAND = os.path.join(test_path, file)
        ret = os.system(RUN_COMMAND)
        if ret == 0:
            num_success += 1
        else:
            failed.append(file)
            num_failed += 1
    
    result_msg = f'@@@@ {build["type"]} tests completed: SUCCESS {num_success}, FAILED {num_failed}\n\n'
    if num_failed > 0:
        print_r(result_msg)
        raise Exception(f'Some tests have failed in {build["type"]} build: {", ".join(failed)}')
    else:
        print_g(result_msg)



def main():
    cwd = os.getcwd()

    if 'run_test.py' not in os.listdir(cwd):
        print_r('\nMake sure to run test in project root\n')
        sys.exit(os.EX_SOFTWARE)
        
    args = sys.argv[1:]
    if len(args) == 0:
        args = ['asan_ubsan', 'tsan_ubsan']
    
    success = True
    for build_name in args:
        
        if build_name not in builds.keys():
            print_r(f'Not a valid sanitizer build name: {build_name}')
            success = False
            continue
        
        try:
            run_one_build(cwd, builds[build_name])
        except Exception as err:
            print_r(err)
            success = False
    
    if not success: 
        sys.exit(os.EX_SOFTWARE)
    sys.exit(os.EX_OK)


if __name__ == '__main__':
    main()
