#pragma once

typedef enum {
  /* original hooks */
  OpenTree,
  OpenTreeEdit,
  RetrieveTree,
  WriteTree,
  CloseTree,
  OpenNCIFileWrite,
  OpenDataFileWrite,
  GetData,
  GetNci,
  PutData,
  PutNci,
  /* hook extensions */
  MakeSegment,
  MakeTimestampedSegment,
  UpdateSegment,
  PutSegment,
  GetSegment,
  GetSegmentRecord,
  PutRow,
  PutTimestampedSegment
} TreeshrHookType;

