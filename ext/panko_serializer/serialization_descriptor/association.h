#include <ruby.h>

#ifndef __ASSOCIATION_H__
#define __ASSOCIATION_H__

#include "serialization_descriptor.h"

typedef struct _Association {
  ID name_id;
  ID link_func_id;
  VALUE name_sym;
  VALUE name_str;

  VALUE rb_descriptor;
  VALUE link_func_sym;
  SerializationDescriptor descriptor;
}* Association;

Association association_read(VALUE association);
void panko_init_association(VALUE mPanko);

#endif
