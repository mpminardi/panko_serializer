#include <ruby.h>
#include "common.h"

VALUE attr_name_for_serialization(Attribute attribute) {
  ID dasherize_id = rb_intern("dasherize");
  volatile VALUE name_str = attribute->name_str;
  if (attribute->alias_name != Qnil) {
    name_str = attribute->alias_name;
  }

  // TODO: this should be configurable
  name_str = rb_funcall(name_str, dasherize_id, 0);

  return name_str;
}
