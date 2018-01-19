public fun getSpiderShotTime(in _shot)
{

   treeopen("spider", _shot);
   _tai_s  = data(build_path("\\SPIDER::TOP.TIMING:ABS_TMSP.PULSE_TIME:TAI_S"));
   _tai_ns = data(build_path("\\SPIDER::TOP.TIMING:ABS_TMSP.PULSE_TIME:TAI_NS"));
   treeclose();

/*
   _epicsTime = ( _tai_s * 1000 - 631152000000Q ) * 1000000;
*/
   _epicsTime = quadword ( ( _tai_ns / 1000000. - 631152000000Q ) * 1000000 );

   return( _epicsTime );
} 

