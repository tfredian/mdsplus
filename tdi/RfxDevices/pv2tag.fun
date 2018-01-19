public fun pv2tag(in _pv)
{
      if($shot <  2017101716)
           _tag=translate("\\"// _pv, '__','-:');
      else
           _tag=translate("\\"// _pv, '__','-:')//":VAL";
       return( build_path(_tag) );

}
