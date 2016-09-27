def deref(arg, levels=1):
  """
  Function to evaluate an expression a specific number of levels.

  This function operates at run-time, rather than compile time as the 
  back-quote operator does.

  When TDI parses an expression containing a back-quote, it replaces the 
  quoted term with the evaluate of that term BEFORE evaluating the 
  expression.  (i.e. this is done at compile time)

  This function is a backquote operator for run-time.  The expression is
  evaluated.  When tdi gets to this function it evaluates the argument
  in the context of the current expression evaluation, a specified number 
  of levels.

  For example using a back-quote in the expression that defines an ident 
  is an error:
    $ tditest
    _a = 42, _b = as_is(_a), `_b
    _a = 42, _b = as_is(_a), `_b
    %TDI Error in EVALUATE(_b)
    %TDI Error in COMPILE("_a = 42, _b = as_is(_a), `_b\n")
    %TDI Syntax error near # marked region
    _a = 42, _b = as_is(_a), `##_b
    ##
    %TDI Error in EXECUTE("_a = 42, _b = as_is(_a), `_b\n")

  But using deref is not:
    $ tditest
    _a = 42, _b = as_is(_a), deref(_b)
    _a = 42, _b = as_is(_a), deref(_b)
    _a
    _c = 42, _d = as_is(_c), deref(_c, 2)
    _c = 42, _d = as_is(_c), deref(_c, 2)
    42
    _e = 42, _f = as_is(_e), deref(_c, 3)
    _e = 42, _f = as_is(_e), deref(_c, 3)
    42
  """
  for level in range(levels):
    arg = arg.evaluate()
  return arg
