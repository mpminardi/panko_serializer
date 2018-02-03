#pragma once

#include "../attribute.h"
#include "ruby.h"

typedef void (*EachAttributeFunc)(VALUE writer, VALUE name, VALUE value);

VALUE attr_name_for_serialization(Attribute attribute);
