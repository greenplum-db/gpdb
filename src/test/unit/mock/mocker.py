#!/usr/bin/env python

import logging
import optparse
import os
import re
import subprocess
import sys

import special

class CFile(object):

    # multi-line comment
    m_comment_pat = re.compile(r'/\*.*?\*/', re.DOTALL)
    # single-line comment (avoid http:// or postgres://)
    s_comment_pat = re.compile(r'(?<!:)//.*$', re.MULTILINE)
    # __attribute__((XXX)): it gets difficult to match arguments.
    # Remove it as it's a noisy keyword for us.
    attribute_pat = re.compile(r'__attribute__\s*\(\((format\s*\([^\)]+\)\s*|format_arg\s*\(\d+\)\s*|.+?)\)\)')

    # function pattern
    func_pat = re.compile(
        # modifier
        r'(?:(static|inline|__inline__|__inline)\s+)*' +
        # rettype
        r'((?:const\s+)?(?:struct\s+|unsigned\s+)?\w+(?:[\s\*]+|\s+))(?:inline\s+|static\s+)?' +
        # funcname
        r'(\w+)\s*'
        # arguments
        r'\(([^{}\)]*?)\)\s*{', re.DOTALL)

    # static variable pattern
    # Currently this requires static keyword at the beginning of line.
    ###staticvar_pat = re.compile(r'^static.+?;', re.MULTILINE | re.DOTALL)
    def __init__(self, path, options):
        self.path = os.path.abspath(path)
        self.options = options
        #with open(self.make_i()) as f:
        with open(self.path) as f:
            self.content = self.strip(f.read())

    def make_i(self):
        """create .i file from .c by using preprocessor with existing make
           system.  The CPPFLAGS may be different from time/env to time/env.
           make will be the best way to preprocess it so far.  Note we need
           not only header file directory but also some definitions.  For
           example some debug symbols may not be found in the existing object
           files if we didn't pass debug #define.
           XXX: Currently we don't need this, but leave it now for future use.
        """
        i_path = '{stem}.i'.format(stem=os.path.splitext(self.path)[0])
        subprocess.check_call(['make', '--quiet', '-C', self.options.src_dir, i_path])
        return i_path

    def strip(self, content):
        """strip comments in the content
        """
        content = CFile.m_comment_pat.sub('', content)
        # backend/libpq/be-secure.c contains private key with '//'
        # backend/utils/misc/guc_gp.c gp_hadoop_connector_jardir has value with '//'
        if 'be-secure' not in self.path and 'guc_gp.c' not in self.path:
            content = CFile.s_comment_pat.sub('', content)
        content = CFile.attribute_pat.sub('', content)
        return content

    def skip_func_body(self, content, index):
        """Skip function body by finding a line starting with a closing brace.
           We wanted to count the number of open/close braces, but some file has
           weird code block based on preprocessor directives.
        """
        pat = re.compile(r'^}\s*$', re.MULTILINE)
        if 'cdbfilerepconnserver' in self.path:
            # FIXIT!: some of the files have unpleasant format.
            pat = re.compile(r'^ ?}', re.MULTILINE)
        m = pat.search(content, index)
        if m:
            if 'cdbgroup' in self.path:
                if content[m.end()+1:].startswith('#endif'):
                    return self.skip_func_body(content, m.end())
            return m.end()
        raise StandardError('unexpected syntax')

    def to_mock(self):
        """Mock up this file.  The basic idea is to replace function body
           with mocked up source.  Other parts are preserved.  Otherwise,
           the source code messed up because of preprocessor directives.
        """
        content = self.content
        prev = 0
        result = ''
        for (func, m) in self.match_functions():
            spos = m.start()
            epos = m.end()
            result += content[prev:spos]
            result += func.to_mock()
            prev = self.skip_func_body(content, epos)
        result += content[prev:]

        return result

    def match_functions(self):
        """Iterator of function pattern matching.
        """
        content = self.content
        for m in CFile.func_pat.finditer(content):
            (modifier, rettype, funcname, args) = m.groups('')
            # 'else if(...){}' looks like a function.  Ignore it.
            if funcname in ['if', 'while', 'switch', 'for', 'foreach',
                            'yysyntax_error', 'defined']:
                continue
            if rettype.strip() in ['define', 'select']:
                continue
            func = FuncSignature(modifier, rettype, funcname, args)
            yield (func, m)

class MockFile(object):

    def __init__(self, cfile, options):
        self.cfile = cfile
        self.options = options
        self.outname = self.output_filename()

    def output_filename(self):
        """outname is src/test/unit/mock/backend/{path}/{stem}_mock.c
        """
        src_dir = self.options.src_dir
        relpath = os.path.relpath(self.cfile.path, src_dir)
        out_dir = self.options.out_dir
        out_dir = os.path.join(out_dir, os.path.dirname(relpath))
        (stem, ext) = os.path.splitext(os.path.basename(relpath))
        if not os.path.exists(out_dir):
            try:
                os.makedirs(out_dir)
            except OSError:
                pass
        return os.path.join(out_dir, '{stem}_mock.c'.format(stem=stem))

    def mock(self):
        outname = self.outname
        with open(outname, 'w') as f:
            f.write("""/*
 *
 * Auto-generated Mocking Source
 *
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

""")
            f.write(self.cfile.to_mock())

            return

class FuncSignature(object):

    # This pattern needs to be fixed; if the argname is not present,
    # we need extra space at the end.
    arg_pat = re.compile(
        # argtype.  i.e. 'const unsigned long', 'struct Foo *', 'const char * const'
        r'((?:register\s+|const\s+|volatile\s+)*(?:enum\s+|struct\s+|unsigned\s+|long\s+)?' +
            r'\w+(?:[\s\*]+)(?:const[\s\*]+)?|\s+)' +
        r'(?:__restrict\s+)?' +
        # argname.  We accept 'arg[]'
        r'([\w\[\]]+)?')
    Variadic = object()

    def __init__(self, modifier, rettype, funcname, args):
        self.modifier = modifier.strip()
        self.rettype = re.sub('inline', '', rettype).strip()
        self.funcname = funcname.strip()
        self.args = self.parse_args(args)

    def is_local(self):
        """Am I a local function?
        """
        return bool(self.modifier)

    def is_pointer_type(self, argtype):
        """Is the type pointer?
        """
        return argtype[-1] == '*'

    def is_variadic(self, arg):
        return arg == FuncSignature.Variadic

    def parse_args(self, arg_string):
        args = []
        arg_string = re.sub(r'\s+', ' ', arg_string)
        if arg_string == 'void' or arg_string == '':
            return args

        for (i, arg) in enumerate(arg_string.split(',')):
            arg = arg.strip()
            # TODO: needs work
            if arg == '...':
                args.append(FuncSignature.Variadic)
                continue
            elif arg == 'PG_FUNCTION_ARGS':
                args.append(('FunctionCallInfo', 'fcinfo'))
                continue
            elif arg == 'SIGNAL_ARGS':
                args.append(('int', 'signal_args'))
                continue

            # general case
            m = FuncSignature.arg_pat.match(arg.strip())
            if not m:
                print '%s %s(%s)' % (self.rettype, self.funcname, arg_string)
            argtype = m.group(1)
            argname = m.group(2) if m.group(2) else 'arg' + str(i)
            args.append((argtype.strip(), argname.strip()))
        return args

    def format_args(self):
        buf = []
        for arg in self.args:
            if self.is_variadic(arg):
                buf.append('...')
                continue
            argtype = arg[0]
            argname = arg[1]
            buf.append(argtype + ' ' + argname)
        if not buf:
            buf = ['void']
        return ', '.join(buf)

    def make_body(self):
        body = special.SpecialFuncs.make_body(self)
        if body:
            return body

        subscript = re.compile('\[\d*\]$')
        # otherwise, general method
        buf = []
        # emit check_expected()
        for arg in self.args:
            if self.is_variadic(arg):
                continue
            argtype = arg[0]
            argname = arg[1]
            ref = '&' if special.ByValStructs.has(argtype) else ''
            argname = subscript.sub('', argname)
            buf.append('\tcheck_expected({ref}{arg});'.format(ref=ref, arg=argname))

        # if the type is pointer, call optional_assignment()
        for arg in self.args:
            if self.is_variadic(arg):
                continue
            elif not self.is_pointer_type(arg[0]):
                continue
            argtype = arg[0]
            argname = arg[1]
            ref = '&' if special.ByValStructs.has(argtype) else ''
            argname = subscript.sub('', argname)
            buf.append('\toptional_assignment({ref}{arg});'.format(ref=ref, arg=argname))

        # Currently, local function doesn't check arguments.
        if self.is_local():
            buf = []

        if special.ByValStructs.has(self.rettype):
            ret = ('\t{rettype} *ret = ({rettype} *) mock();\n' +
                  '\treturn *ret;').format(rettype=self.rettype)
        elif self.rettype != 'void':
            ret = '\treturn ({cast}) mock();'.format(cast=self.rettype)
        else:
            ret = '\tmock();'
        buf.append(ret)
        return '\n'.join(buf)

    def to_mock(self):
        mod_ret = self.rettype
        if self.modifier:
            mod_ret = self.modifier + ' ' + mod_ret
        return """
{mod_ret}
{name}({args})
{{
{body}
}}
""".format(mod_ret=mod_ret, name=self.funcname, args=self.format_args(),
           body=self.make_body())

def main():
    logging.basicConfig(level=logging.INFO)
    try:
        mydir = os.path.dirname(os.path.realpath(__file__))
        parser = optparse.OptionParser()
        parser.add_option('--out-dir',
                          dest='out_dir',
                          default=os.path.join(mydir, '.'))
        parser.add_option('--src-dir',
                          dest='src_dir',
                          default=os.path.join(mydir, '../../..'))
        (options, args) = parser.parse_args()

        if len(args) < 1:
            parser.error('insufficient arguments')

        cfile = CFile(args[0], options)
        mock = MockFile(cfile, options)
        mock.mock()
    except Exception as e:
        logging.error('Error has occurred during parsing %s: %s' % (args[0], str(e)))
        raise

if __name__ == '__main__':
    main()
