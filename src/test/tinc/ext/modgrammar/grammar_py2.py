from modgrammar import GrammarClass, GrammarParser, InternalError, UnknownReferenceError
from modgrammar import util

class Grammar(object):
    """
    This class is not intended to be instantiated directly.  Instead, it is a base class to be used for defining your own custom grammars.

    Subclasses of :class:`Grammar` serve two purposes.  The first is to actually define the grammar used for parsing.  The second is to serve as a base type for result objects created as the result of parsing.

    To define a new grammar, you should create a new class definition, descended from :class:`Grammar`.  In this class definition, you can override several class attributes and class methods to customize the behavior of the grammar.
    """
    __metaclass__ = GrammarClass

    grammar = ()
    grammar_terminal = False
    grammar_collapse = False
    grammar_greedy = True
    grammar_null_subtoken_ok = True
    grammar_whitespace = None
    grammar_error_override = False
    grammar_hashattrs = (
        'grammar_name', 'grammar', 'grammar_min', 'grammar_max', 'grammar_collapse', 'grammar_greedy', 'grammar_whitespace')

    @classmethod
    def __class_init__(cls, attrs):
        cls.grammar_min = len(cls.grammar)
        cls.grammar_max = len(cls.grammar)

    @classmethod
    def parser(cls, sessiondata=None, tabs=1):
        """
        Return a :class:`GrammarParser` associated with this grammar.

        If provided, *sessiondata* can contain data which should be provided to the :meth:`elem_init` method of each result object created during parsing.

        The *tabs* parameter indicates the width of "tab stops" in the input (i.e. how far a "tab" character will advance the column position when encountered).  This is only used to correctly report column numbers in :exc:`ParseError`\ s.  If you don't care about that, or your input does not contain tabs, you can ignore this parameter.
        """
        return GrammarParser(cls, sessiondata, tabs)

    # Yields:
    #   Success:     (count, obj)
    #   Incomplete:  (None, None)
    #   Parse error: (False, error_tuple)
    @classmethod
    def grammar_parse(cls, text, index, sessiondata):
        """
        This method is called by the :mod:`modgrammar` parser system to actually attempt to match this grammar against a piece of text.  This method is not intended to be called directly by an application (use the :meth:`parser` method to obtain a :class:`GrammarParser` object and use that).  In advanced cases, this method can be overridden to provide custom parsing behaviors for a particular grammar type.

        NOTE: Overriding this method is very complicated and currently beyond the scope of this documentation.  This is not recommended for anyone who does not understand the :mod:`modgrammar` parser design very well.  (Someday, with luck, there will be some more documentation written on this advanced topic.)
        """

        grammar = cls.grammar
        grammar_min = cls.grammar_min
        grammar_max = cls.grammar_max
        greedy = cls.grammar_greedy
        if cls.grammar_whitespace is True:
            whitespace_re = util._whitespace_re
        else:
            whitespace_re = cls.grammar_whitespace
        objs = []
        states = []
        positions = []
        best_error = None
        pos = index
        first_pos = None

        while True:
            # Forward ho!
            while True:
                if not greedy and len(objs) >= grammar_min:
                    # If we're not "greedy", then try returning every match as soon as we
                    # get it (which will naturally return the shortest matches first)
                    yield (pos - index, cls(text.string, index, pos, objs))
                    # We need to copy objs for any further stuff, since it's now part of
                    # the object we yielded above, which our caller may be keeping for
                    # later, so if we modify it in-place we'll be screwing up the
                    # 'entities' list of that object in the process.
                    objs = list(objs)
                if len(objs) >= grammar_max:
                    break
                prews_pos = pos
                if whitespace_re:
                    while True:
                        m = whitespace_re.match(text.string, pos)
                        if m:
                            pos = m.end()
                        if pos < len(text.string) or text.eof:
                            break
                        text = yield (None, None)
                if first_pos is None:
                    first_pos = pos
                s = grammar[len(objs)].grammar_parse(text, pos, sessiondata)
                offset, obj = next(s)
                while offset is None:
                    if text.eof:
                        # Subgrammars should not be asking for more data after eof.
                        raise InternalError("{0} requested more data when at EOF".format(grammar[len(objs)]))
                    text = yield (None, None)
                    offset, obj = s.send(text)
                if offset is False:
                    best_error = util.update_best_error(best_error, obj)
                    pos = prews_pos
                    break
                objs.append(obj)
                states.append((pos, s))
                pos += offset
                # Went as far as we can forward and it didn't work.  Backtrack until we
            # find something else to follow...
            while True:
                if greedy and len(objs) >= grammar_min:
                    # If we are greedy, then return matches only after we've gone as far
                    # forward as possible, while we're backtracking (returns the longest
                    # matches first)
                    yield (pos - index, cls(text.string, index, pos, objs))
                    # We need to copy objs for any further stuff, since it's now part of
                    # the object we yielded above, which our caller may be keeping for
                    # later, so if we modify it in-place we'll be screwing up the
                    # 'entities' list of that object in the process.
                    objs = list(objs)
                if not states:
                    break
                pos, s = states[-1]
                offset, obj = next(s)
                while offset is None:
                    if text.eof:
                        # Subgrammars should not be asking for more data after eof.
                        raise InternalError("{0} requested more data when at EOF".format(grammar[len(objs) - 1]))
                    text = yield (None, None)
                    offset, obj = s.send(text)
                if offset is False:
                    best_error = util.update_best_error(best_error, obj)
                    states.pop()
                    objs.pop()
                else:
                    objs[-1] = obj
                    pos += offset
                    break
                    # Have we gone all the way back to the beginning?
                    # If so, give up.  If not, loop around and try going forward again.
            if not states:
                break
        if cls.grammar_error_override:
            # If our sub-grammars failed to match, but we've got
            # grammar_error_override set, return ourselves as the failed match
            # grammar instead.
            yield util.error_result(index, cls)
        elif ((len(cls.grammar) == 1)
              and (best_error[0] == first_pos)
              and (cls.grammar_desc != cls.grammar_name) ):
            # We're just a simple wrapper (i.e. an alias) around another single
            # grammar class, and it failed to match, and we have a custom
            # grammar_desc.  Return ourselves as the failed match grammar so the
            # ParseError will contain our grammar_desc instead.
            yield util.error_result(index, cls)
        else:
            yield util.error_result(*best_error)

    @classmethod
    def grammar_ebnf_lhs(cls, opts):
        """
        Determines the string to be used for this grammar when it occurs in the left-hand-side (LHS) of an EBNF definition.  This can be overridden to customize how this grammar is represented by :func:`generate_ebnf`.

        Returns a tuple *(string, grammars)*, where *string* is the EBNF LHS string to use, and *grammars* is a list of other grammars on which this one depends (i.e. grammars whose names are referenced in *string*).
        """

        return (cls.grammar_name, ())

    @classmethod
    def grammar_ebnf_rhs(cls, opts):
        """
        Determines the string to be used to describe this grammar when it occurs in the right-hand-side (RHS) of an EBNF definition.  This can be overridden to customize how this grammar is represented by :func:`generate_ebnf`.

        Returns a tuple *(string, grammars)*, where *string* is the EBNF RHS string to use, and *grammars* is a list of other grammars on which this one depends (i.e. grammars whose names are referenced in *string*).
        """

        if cls.grammar_terminal and not opts["expand_terminals"]:
            return None
        if cls.grammar_parse.__func__ is Grammar.grammar_parse.__func__:
            names, nts = util.get_ebnf_names(cls.grammar, opts)
            return (", ".join(names), nts)
        else:
            return (util.ebnf_specialseq(cls, opts), ())

    @classmethod
    def grammar_details(cls, depth=-1, visited=None):
        """
        Returns a string containing a description of the contents of this grammar definition (such as used by :func:`repr`).

        *depth* specifies a recursion depth to use when constructing the string description.  If *depth* is nonzero, this method will recursively call :meth:`grammar_details` for each of its sub-grammars in turn to construct the final description.  If *depth* is zero, this method will just return this grammar's name (:attr:`grammar_name`).  If *depth* is negative, recursion depth is not limited.

        *visited* is used for detecting circular references during recursion.  It can contain a tuple of grammars which have already been seen and should not be descended into again.
        """

        if cls.grammar_parse.__func__ is Grammar.grammar_parse.__func__:
            if depth != 0:
                if not visited:
                    visited = (cls,)
                elif cls in visited:
                    # Circular reference.  Stop here.
                    return cls.grammar_name
                else:
                    visited = visited + (cls,)
                if len(cls.grammar) == 1:
                    return cls.grammar[0].grammar_details(depth - 1, visited)
                else:
                    return "(" + ", ".join((g.grammar_details(depth - 1, visited) for g in cls.grammar)) + ")"
        return cls.grammar_name

    @classmethod
    def __class_str__(cls):
        """
        Returns the string to be used when :func:`str` is used on this grammar class (Note: This is for the class itself, not for instances of the class.  For those, the usual :meth:`__str__` is used).
        """

        return cls.grammar_name

    @classmethod
    def __class_repr__(cls):
        """
        Returns the string to be used when :func:`repr` is used on this grammar class (Note: This is for the class itself, not for instances of the class.  For those, the usual :meth:`__repr__` is used).
        """

        name = cls.grammar_name
        details = cls.grammar_details(1)
        if name == details or name.startswith("<"):
            return "<Grammar: {0}>".format(details)
        else:
            return "<Grammar[{0}]: {1}>".format(name, details)

    @classmethod
    def grammar_hashdata(cls):
        return (cls.grammar_parse.__func__, tuple(getattr(cls, x) for x in cls.grammar_hashattrs))

    @classmethod
    def grammar_resolve_refs(cls, refmap={}, recurse=True, follow=False, missing_ok=False, skip=None):
        """
        Resolve any :func:`REF` declarations within the grammar and replace them with the actual sub-grammars they refer to.  The following optional arguments can be provided:

          *refmap*
            If provided, contains a dictionary of reference-name to grammar mappings to use.  If a reference's name is found in this dictionary, the dictionary value will be used to replace it, instead of using the standard name-lookup procedure.
          *recurse*
            If set to :const:`True`, will perform a recursive search into each of this grammar's sub-grammars, calling :meth:`grammar_resolve_refs` on each with the same parameters.
          *follow*
            If set to :const:`True` (and *recurse* is also :const:`True`), will also call :meth:`grammar_resolve_refs` on the result of each :func:`REF` after it is resolved.
          *missing_ok*
            If :const:`True`, it is not considered an error if a :func:`REF` construct cannot be resolved at this time (it will simply be left as a :func:`REF` in the resulting grammar).  If :const:`False`, then all references must be resolvable or an :exc:`UnresolvedReference` exception will be raised.
          *skip*
            An optional list of grammars which should not be searched for :func:`REF` constructs (useful in conjunction with *recurse* to exclude certain parts of the grammar).
        """

        from modgrammar import Reference

        if not skip:
            skip = set()
        else:
            # We maintain 'skip' as a set of ids, because keeping the objects
            # themselves will call __hash__ on them, which makes the 'grammar'
            # attribute immutable, which means we can't do what we need to do.
            skip = set(x if isinstance(x, int) else id(x) for x in skip)
        if id(cls) in skip:
            return
        skip.add(id(cls))
        grammar = []
        for g in cls.grammar:
            rec = recurse
            while issubclass(g, Reference):
                try:
                    g = g.resolve(refmap)
                except UnknownReferenceError:
                    if not missing_ok:
                        raise
                if not follow:
                    rec = False
                    break
            grammar.append(g)
            if rec and hasattr(g, "grammar_resolve_refs"):
                g.grammar_resolve_refs(refmap, recurse, follow, missing_ok, skip)
        cls.grammar = tuple(grammar)

    def __init__(self, string, start=0, end=None, parsed=()):
        self._str_info = (string, start, end)
        self.elements = parsed
        self.string = ""

    def grammar_collapsed_elems(self, sessiondata):
        """
        *Note: This is an instance method, not a classmethod*

        Return the list of elements to be used in place of this one when collapsing (this is only used if :attr:`grammar_collapse` is :const:`True`).
        """
        elems = []
        if not self.elements:
            return (None,)
        for e in self.elements:
            if not getattr(e, "grammar_collapse_skip", False):
                elems.append(e)
        if elems:
            return elems
        else:
            return self.elements

    def grammar_postprocess(self, parent, sessiondata):
        self.parent = parent
        if hasattr(self, '_str_info'):
            s, start, end = self._str_info
            self.string = s[start:end]
            #del self._str_info
            if self.grammar_collapse:
                elems = self.grammar_collapsed_elems(sessiondata)
                pp_elems = []
                for e in elems:
                    if e is None:
                        pp_elems.append(e)
                    else:
                        pp_elems.extend(e.grammar_postprocess(parent, sessiondata))
                return tuple(pp_elems)
            else:
                elems = self.elements
                pp_elems = []
                for e in elems:
                    pp_elems.extend(e.grammar_postprocess(self, sessiondata))
                self.elements = tuple(pp_elems)
                del self._str_info
        self.elem_init(sessiondata)
        return (self,)

    def elem_init(self, sessiondata):
        """
        This method is called on each result object after it is fully initialized, before the resulting parse tree is returned to the caller.  It can be overridden to perform any custom initialization desired (the default implementation does nothing).
        """
        pass

    def get_all(self, *type_path):
        """
        Return all immediate sub-elements of the given type.

        If more than one type parameter is provided, it will treat the types as a "path" to traverse: for all sub-elements matching the first type, retrieve all sub-elements of those matching the second type, and so on, until it reaches the last type in the list.  (It will thus return all elements of the parse tree which are of the final type, which can be reached by traversing the previous types in order.)
        """
        return list(self._search_recursive(isinstance, False, type_path))

    def get(self, *type_path):
        """
        Return the first immediate sub-element of the given type (or by descending through multiple types, in the same way as :meth:`get_all`).

        This is equivalent to ``.get_all(*type_path)[0]`` except that it is more efficient, and will return :const:`None` if there are no such objects (instead of raising :exc:`IndexError`).
        """
        try:
            return next(self._search_recursive(isinstance, False, type_path))
        except StopIteration:
            return None

    def find_all(self, *type_path):
        """
        Return all elements anywhere in the parse tree matching the given type.

        Similar to :meth:`get_all`, if more than one type parameter is provided, it will treat the types as a "path" to traverse in order.  The difference from :meth:`get_all` is that, for each step in the path, the elements found do not have to be direct sub-elements, but can be anywhere in the sub-tree.
        """
        return list(self._search_recursive(isinstance, True, type_path))

    def find(self, *type_path):
        """
        Return the first element anywhere in the parse tree matching the given type (or by descending through multiple types, in the same way as :meth:`find_all`).

        This is equivalent to ``.find_all(*type_path)[0]`` except that it is more efficient, and will return :const:`None` if there are no such objects (instead of raising :exc:`IndexError`).
        """
        try:
            return next(self._search_recursive(isinstance, True, type_path))
        except StopIteration:
            return None

    def find_tag_all(self, *tag_path):
        """
        Return all elements anywhere in the parse tree with the given tag.

        This functions identically to :meth:`find_all`, except that the criteria for matching is based on tags, rather than object types.
        """
        func = lambda e, l: l in getattr(e, "grammar_tags", ())
        return list(self._search_recursive(func, True, tag_path))

    def find_tag(self, *tag_path):
        """
        Return the first element anywhere in the parse tree with the given tag (or by descending through multiple tags, in the same way as :meth:`find_tag_all`).

        This is equivalent to ``.find_tag_all(*tag_path)[0]`` except that it is more efficient, and will return :const:`None` if there are no such objects (instead of raising :exc:`IndexError`).
        """
        func = lambda e, l: l in getattr(e, "grammar_tags", ())
        try:
            return next(self._search_recursive(func, True, tag_path))
        except StopIteration:
            return None

    def _search_recursive(self, func, skip, args):
        subargs = list(args)
        a = subargs.pop(0)
        for e in self.elements:
            if func(e, a):
                if subargs:
                    for result in e._search_recursive(func, skip, subargs):
                        yield result
                else:
                    yield e
            elif skip and e is not None:
                for o in e._search_recursive(func, skip, args):
                    yield o

    def terminals(self):
        """
        Return an ordered list of all result objects in the parse tree which are terminals (that is, where :attr:`grammar_terminal` is :const:`True`).
        """
        if self.grammar_terminal:
            return [self]
        results = []
        for e in self.elements:
            if e is None:
                pass
            else:
                results.extend(e.terminals())
        return results

    def tokens(self):
        """
        Return the parsed string, broken down into its smallest grammatical components.  (Another way of looking at this is that it returns the string values of all of the :meth:`terminals`.)
        """
        return [e.string for e in self.terminals()]

    def __getitem__(self, index):
        return self.elements[index]

    def __len__(self):
        return len(self.string)

    def __nonzero__(self):
        return bool(self.elements) or self.grammar_terminal

    def __str__(self):
        return self.string

    def __repr__(self):
        name = self.__class__.grammar_name
        details = [repr(str(e) if e is not None else e) for e in self.elements]
        if not details:
            details = (repr(self.string),)
        return "{0}<{1}>".format(name, ", ".join(details))
