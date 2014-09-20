#!/usr/bin/env python3

import sys

strafe_ang_func = False

for line in sys.stdin:
    line = line.strip()
    if line.startswith('//') or line.startswith('#') or not line:
        continue

    tokens = line.split()
    if tokens[0] == '@U':
        try:
            nwait = int(tokens[1])
            niter = int(tokens[2])
            usewait = 1
            if len(tokens) >= 4:
                usewait = int(tokens[3])
            for _ in range(niter):
                print('wait\n' * nwait, end='')
                print('+use')
                print('wait\n' * usewait, end='')
                print('-use')
        except ValueError:
            print('Wrong argument type to @U', file=sys.stderr)
            sys.exit(1)
        except IndexError:
            print('@U needs two or three arguments', file=sys.stderr)
            sys.exit(1)
        continue

    if tokens[0] == 'tas_sba' or tokens[0] == 'tas_s2y':
        strafe_ang_func = True
        print(line)
        continue

    try:
        evalstack = []
        for token in tokens:
            if token == '+':
                evalstack[-2:] = [evalstack[-2] + evalstack[-1]]
            elif token == '-':
                evalstack[-2:] = [evalstack[-2] - evalstack[-1]]
            else:
                evalstack.append(int(token))
    except (ValueError, IndexError):
        print(line)
        if strafe_ang_func and line == 'wait':
            strafe_ang_func = False
            print('exec waitscript.cfg')
        continue

    if len(evalstack) != 1:
        print('Wrong number of operators:', line, file=sys.stderr)
        sys.exit(1)

    if evalstack[0] < 1:
        print('Expression evaluates to < 1:', line, file=sys.stderr)
        sys.exit(1)

    if strafe_ang_func:
        strafe_ang_func = False
        evalstack[0] -= 1
        print('wait')
        print('exec waitscript.cfg')
    print('wait\n' * evalstack[0], end='')
