public fun rfft(in _sig, optional in _npoint) {

  public __n =  size(_sig);

  if ( !present( _npoint ) )
     _npoint = __n;

  public __sig = data(_sig)[ 0 : _npoint ];
  public __te = dim_of(_sig)[ _npoint ] / __n;
  public __afft=*;
  public __ffft=*;
  py(["from MDSplus import Data,makeData", 
      "from numpy import abs, fft ", 
      "afft=abs(fft.rfft( Data.getTdiVar('__sig')))",
      "afft[0]=0",
      "makeData( afft ).setTdiVar('__afft')", 
      "makeData( fft.rfftfreq( Data.getTdiVar('__n').data(),Data.getTdiVar('__te').data() ) ).setTdiVar('__ffft')"]);
  return(make_signal( public __afft,, public __ffft));
}



