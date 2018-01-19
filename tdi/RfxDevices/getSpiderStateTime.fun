public fun getSpiderStateTime(in _spiderStateCode)
{

   _data = data(build_path("\\SPIDER_TREND::TOP.GVS_TREND.SIGV.FSM.ST.STAT:VAL"));
   _time = dim_of(build_path("\\SPIDER_TREND::TOP.GVS_TREND.SIGV.FSM.ST.STAT:VAL"));

   _stateTime = sum((_data == _spiderStateCode) * _time);

   return( _stateTime );
} 

