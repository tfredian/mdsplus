public fun GetXnciSegment(as_is _node, in _xnciname, in _idx) {
  _nid=getnci(_node,"NID_NUMBER");
  _data=*;
  _dim=*;
  _status=TreeShr->TreeGetXNciSegment(val(_nid),ref(_xnciname),val(_idx),xd(_data),xd(_dim));
  if (_status & 1) {
    _scl=*;
    if(TreeShr->TreeGetSegmentScale(val(_nid),xd(_scl))&1 && KIND(_scl)!=0) {
      return(make_signal(_scl,_data,_dim));
    } else {
      return(make_signal(_data,*,_dim));
    }
  } else {
    return(*);
  }
}
