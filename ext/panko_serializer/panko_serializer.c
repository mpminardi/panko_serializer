#include "panko_serializer.h"

#include <ruby.h>

static ID push_value_id;
static ID push_array_id;
static ID push_object_id;
static ID push_json_id;
static ID pop_id;

static ID to_a_id;

static ID object_id;
static ID serialization_context_id;
static ID id_id;
static ID type_id;
static ID dasherize_id;

static VALUE SKIP = Qundef;
static VALUE DATA_STR = Qundef;
static VALUE ATTRIBUTES_STR = Qundef;
static VALUE TYPE_STR = Qundef;
static VALUE RELATIONSHIPS_STR = Qundef;
static VALUE ID_STR = Qundef;
static VALUE LINKS_STR = Qundef;

void write_value(VALUE str_writer, VALUE key, VALUE value, VALUE isJson) {
  volatile VALUE dasherized_name = rb_funcall(key, dasherize_id, 0); // TODO: make configurable
  if (isJson == Qtrue) {
    rb_funcall(str_writer, push_json_id, 2, value, dasherized_name);
  } else {
    rb_funcall(str_writer, push_value_id, 2, value, dasherized_name);
  }
}

void serialize_method_fields(VALUE object, VALUE str_writer,
                             SerializationDescriptor descriptor) {
  if (RARRAY_LEN(descriptor->method_fields) == 0) {
    return;
  }

  volatile VALUE method_fields, serializer;
  long i;

  method_fields = descriptor->method_fields;

  serializer = descriptor->serializer;
  rb_ivar_set(serializer, object_id, object);

  for (i = 0; i < RARRAY_LEN(method_fields); i++) {
    volatile VALUE raw_attribute = RARRAY_AREF(method_fields, i);
    Attribute attribute = PANKO_ATTRIBUTE_READ(raw_attribute);

    volatile VALUE result = rb_funcall(serializer, attribute->name_id, 0);
    if (result != SKIP) {
      write_value(str_writer, attribute->name_str, result, Qfalse);
    }
  }

  rb_ivar_set(serializer, object_id, Qnil);
}

void serialize_fields(VALUE object, VALUE str_writer,
                      SerializationDescriptor descriptor) {
  descriptor->attributes_writer.write_attributes(object, descriptor->attributes,
                                                 write_value, str_writer);

  serialize_method_fields(object, str_writer, descriptor);
}

void serialize_has_one_associations(VALUE object, VALUE str_writer,
                                    VALUE associations) {
  long i;
  for (i = 0; i < RARRAY_LEN(associations); i++) {
    volatile VALUE association_el = RARRAY_AREF(associations, i);
    Association association = association_read(association_el);

    volatile VALUE value = rb_funcall(object, association->name_id, 0);

    if (NIL_P(value)) {
      write_value(str_writer, association->name_str, value, Qfalse);
    } else {
      serialize_object(association->name_str, value, str_writer,
                       association->descriptor);
    }
  }
}

void serialize_has_many_associations(VALUE object, VALUE str_writer,
                                     VALUE associations) {
  long i;
  for (i = 0; i < RARRAY_LEN(associations); i++) {
    volatile VALUE association_el = RARRAY_AREF(associations, i);
    Association association = association_read(association_el);

    volatile VALUE value = rb_funcall(object, association->name_id, 0);

    if (NIL_P(value)) {
      write_value(str_writer, association->name_str, value, Qfalse);
    } else {
      serialize_objects(association->name_str, value, str_writer,
                        association->descriptor);
    }
  }
}


void serialize_links(VALUE object, VALUE str_writer, SerializationDescriptor descriptor) {
  if (RARRAY_LEN(descriptor->links) == 0) {
    return;
  }

  rb_funcall(str_writer, push_object_id, 1, LINKS_STR);


  volatile VALUE links, serializer;
  long i;

  links = descriptor->links;

  serializer = descriptor->serializer;
  rb_ivar_set(serializer, object_id, object);

  for (i = 0; i < RARRAY_LEN(links); i++) {
    volatile VALUE raw_attribute = RARRAY_AREF(links, i);
    Attribute attribute = PANKO_ATTRIBUTE_READ(raw_attribute);

    volatile VALUE result = rb_funcall(serializer, attribute->name_id, 0);
    if (result != SKIP) {
      write_value(str_writer, attribute->name_str, result, Qfalse);
    }
  }

  rb_ivar_set(serializer, object_id, Qnil);

  rb_funcall(str_writer, pop_id, 0);
}

void serialize_relationship(VALUE object, VALUE original_object, VALUE str_writer, Association association, VALUE original_serializer) {
    rb_funcall(str_writer, push_object_id, 1, association->name_str);
    rb_funcall(str_writer, push_object_id, 1, DATA_STR);

    volatile VALUE link_func, serializer;

    serializer = association->descriptor->serializer;
    link_func = association->link_func_sym;

    rb_ivar_set(serializer, object_id, object);
    write_value(str_writer, TYPE_STR, rb_funcall(serializer, type_id, 0), Qfalse);
    write_value(str_writer, ID_STR, rb_funcall(serializer, id_id, 0), Qfalse);
    rb_ivar_set(serializer, object_id, Qnil);

    rb_funcall(str_writer, pop_id, 0);

    if (!NIL_P(association->link_func_sym)) {
      volatile VALUE result;

      rb_ivar_set(original_serializer, object_id, original_object);

      result = rb_funcall(original_serializer, association->link_func_id, 0);
      if (result != SKIP) {
        // TODO:
        write_value(str_writer, LINKS_STR, result, Qfalse);
      }

      rb_ivar_set(original_serializer, object_id, Qnil);
    }

    rb_funcall(str_writer, pop_id, 0);
}

void serialize_has_one_associations_jsonapi(VALUE object, VALUE str_writer,
                                    VALUE associations, VALUE container_serializer) {
  long i;
  for (i = 0; i < RARRAY_LEN(associations); i++) {
    volatile VALUE association_el = RARRAY_AREF(associations, i);
    Association association = association_read(association_el);

    volatile VALUE value = rb_funcall(object, association->name_id, 0);

    if (NIL_P(value)) {
      write_value(str_writer, association->name_str, value, Qfalse);
    } else {
      serialize_relationship(value, object, str_writer, association, container_serializer);
    }
  }
}

void serialize_relationships_internal(VALUE objects, VALUE original_object, VALUE str_writer, Association association, VALUE original_serializer) {
    rb_funcall(str_writer, push_object_id, 1, association->name_str);
    rb_funcall(str_writer, push_array_id, 1, DATA_STR);

    volatile VALUE link_func, serializer;

    serializer = association->descriptor->serializer;
    link_func = association->link_func_sym;

    if (!RB_TYPE_P(objects, T_ARRAY)) {
      objects = rb_funcall(objects, to_a_id, 0);
    }

    long i;
    for (i = 0; i < RARRAY_LEN(objects); i++) {
      volatile VALUE object = RARRAY_AREF(objects, i);
      rb_funcall(str_writer, push_object_id, 1, Qnil);
      rb_ivar_set(serializer, object_id, object);
      write_value(str_writer, TYPE_STR, rb_funcall(serializer, type_id, 0), Qfalse);
      write_value(str_writer, ID_STR, rb_funcall(serializer, id_id, 0), Qfalse);
      rb_ivar_set(serializer, object_id, Qnil);

      // TODO: probably need to move this one level out too?
      if (!NIL_P(association->link_func_sym)) {
        volatile VALUE result;

        rb_ivar_set(original_serializer, object_id, original_object);

        result = rb_funcall(original_serializer, association->link_func_id, 0);
        if (result != SKIP) {
          // TODO:
          write_value(str_writer, LINKS_STR, result, Qfalse);
        }

        rb_ivar_set(original_serializer, object_id, Qnil);
      }
      rb_funcall(str_writer, pop_id, 0);
    }

    rb_funcall(str_writer, pop_id, 0);
    rb_funcall(str_writer, pop_id, 0);
}

void serialize_has_many_associations_jsonapi(VALUE object, VALUE str_writer,
                                     VALUE associations, VALUE container_serializer) {
  long i;
  for (i = 0; i < RARRAY_LEN(associations); i++) {
    volatile VALUE association_el = RARRAY_AREF(associations, i);
    Association association = association_read(association_el);

    volatile VALUE value = rb_funcall(object, association->name_id, 0);

    if (NIL_P(value)) {
      // TODO: need empty list here
      write_value(str_writer, association->name_str, value, Qfalse);
    } else {
      // TODO: need to have serialize_relationship loop through or write another function
      serialize_relationships_internal(value, object, str_writer, association, container_serializer);
    }
  }
}

VALUE serialize_object(VALUE key, VALUE object, VALUE str_writer,
                       SerializationDescriptor descriptor) {
  sd_set_writer(descriptor, object);

  rb_funcall(str_writer, push_object_id, 1, key);

  serialize_fields(object, str_writer, descriptor);

  if (RARRAY_LEN(descriptor->has_one_associations) > 0) {
    serialize_has_one_associations(object, str_writer,
                                   descriptor->has_one_associations);
  }

  if (RARRAY_LEN(descriptor->has_many_associations) > 0) {
    serialize_has_many_associations(object, str_writer,
                                    descriptor->has_many_associations);
  }

  rb_funcall(str_writer, pop_id, 0);

  return Qnil;
}

VALUE serialize_objects(VALUE key, VALUE objects, VALUE str_writer,
                        SerializationDescriptor descriptor) {
  long i;

  rb_funcall(str_writer, push_array_id, 1, key);

  if (!RB_TYPE_P(objects, T_ARRAY)) {
    objects = rb_funcall(objects, to_a_id, 0);
  }

  for (i = 0; i < RARRAY_LEN(objects); i++) {
    volatile VALUE object = RARRAY_AREF(objects, i);
    serialize_object(Qnil, object, str_writer, descriptor);
  }

  rb_funcall(str_writer, pop_id, 0);

  return Qnil;
}

VALUE serialize_attributes(VALUE object, VALUE str_writer, SerializationDescriptor descriptor) {
  rb_funcall(str_writer, push_object_id, 1, ATTRIBUTES_STR);

  serialize_fields(object, str_writer, descriptor);

  rb_funcall(str_writer, pop_id, 0);

  return Qnil;
}

VALUE serialize_relationships(VALUE object , VALUE str_writer, SerializationDescriptor descriptor) {
  rb_funcall(str_writer, push_object_id, 1, RELATIONSHIPS_STR);

  if (RARRAY_LEN(descriptor->has_one_associations) > 0) {
    serialize_has_one_associations_jsonapi(object, str_writer,
                                   descriptor->has_one_associations,
                                   descriptor->serializer);
  }


  if (RARRAY_LEN(descriptor->has_many_associations) > 0) {
    serialize_has_many_associations_jsonapi(object, str_writer,
                                    descriptor->has_many_associations,
                                    descriptor->serializer);
  }

  rb_funcall(str_writer, pop_id, 0);

  return Qnil;
}

VALUE serialize_object_jsonapi_internal(VALUE object, VALUE str_writer,
                       SerializationDescriptor descriptor) {
  sd_set_writer(descriptor, object);

  rb_ivar_set(descriptor->serializer, object_id, object);
  write_value(str_writer, ID_STR, rb_funcall(descriptor->serializer, id_id, 0), Qfalse);
  write_value(str_writer, TYPE_STR, rb_funcall(descriptor->serializer, type_id, 0), Qfalse);
  rb_ivar_set(descriptor->serializer, object_id, Qnil);

  serialize_attributes(object, str_writer, descriptor);

  serialize_relationships(object, str_writer, descriptor);

  // TODO: define this on the relationship too somehow? don't think we want
  // links _from_ relationships either, just id / type
  serialize_links(object, str_writer, descriptor);

  return Qnil;
}

VALUE serialize_object_jsonapi(VALUE object, VALUE str_writer,
                       SerializationDescriptor descriptor) {
  rb_funcall(str_writer, push_object_id, 1, Qnil);
  rb_funcall(str_writer, push_object_id, 1, DATA_STR);

  serialize_object_jsonapi_internal(object, str_writer, descriptor);

  rb_funcall(str_writer, pop_id, 0);
  rb_funcall(str_writer, pop_id, 0);
  return Qnil;
}

VALUE serialize_objects_jsonapi(VALUE key, VALUE objects, VALUE str_writer,
                        SerializationDescriptor descriptor) {
  long i;

  rb_funcall(str_writer, push_object_id, 1, Qnil);
  rb_funcall(str_writer, push_array_id, 1, DATA_STR);

  if (!RB_TYPE_P(objects, T_ARRAY)) {
    objects = rb_funcall(objects, to_a_id, 0);
  }

  for (i = 0; i < RARRAY_LEN(objects); i++) {
    volatile VALUE object = RARRAY_AREF(objects, i);
    rb_funcall(str_writer, push_object_id, 1, Qnil);
    serialize_object_jsonapi_internal(object, str_writer, descriptor);
    rb_funcall(str_writer, pop_id, 0);
  }

  rb_funcall(str_writer, pop_id, 0);
  rb_funcall(str_writer, pop_id, 0);
  return Qnil;
}

VALUE serialize_object_api(VALUE klass, VALUE object, VALUE str_writer,
                           VALUE descriptor) {
  SerializationDescriptor sd = sd_read(descriptor);
  return serialize_object(Qnil, object, str_writer, sd);
}

VALUE serialize_objects_api(VALUE klass, VALUE objects, VALUE str_writer,
                            VALUE descriptor) {
  serialize_objects(Qnil, objects, str_writer, sd_read(descriptor));

  return Qnil;
}

VALUE serialize_object_api_jsonapi(VALUE klass, VALUE object, VALUE str_writer,
                           VALUE descriptor) {
  SerializationDescriptor sd = sd_read(descriptor);
  return serialize_object_jsonapi(object, str_writer, sd);
}

VALUE serialize_objects_api_jsonapi(VALUE klass, VALUE object, VALUE str_writer,
                           VALUE descriptor) {
  SerializationDescriptor sd = sd_read(descriptor);
  return serialize_objects_jsonapi(DATA_STR, object, str_writer, sd);
}


void Init_panko_serializer() {
  push_value_id = rb_intern("push_value");
  push_array_id = rb_intern("push_array");
  push_object_id = rb_intern("push_object");
  push_json_id = rb_intern("push_json");
  pop_id = rb_intern("pop");
  to_a_id = rb_intern("to_a");
  object_id = rb_intern("@object");
  id_id = rb_intern("id");
  type_id = rb_intern("type");
  serialization_context_id = rb_intern("@serialization_context");
  dasherize_id = rb_intern("dasherize"); // TODO: actually use this

  VALUE mPanko = rb_define_module("Panko");

  rb_define_singleton_method(mPanko, "serialize_object", serialize_object_api,
                             3);

  rb_define_singleton_method(mPanko, "serialize_objects", serialize_objects_api,
                             3);
  rb_define_singleton_method(mPanko, "serialize_object_jsonapi", serialize_object_api_jsonapi,
                             3);
  rb_define_singleton_method(mPanko, "serialize_objects_jsonapi", serialize_objects_api_jsonapi,
                             3);

  VALUE mPankoSerializer = rb_const_get(mPanko, rb_intern("Serializer"));
  SKIP = rb_const_get(mPankoSerializer, rb_intern("SKIP"));
  rb_global_variable(&SKIP);
  DATA_STR = rb_const_get(mPankoSerializer, rb_intern("DATA_STR"));
  rb_global_variable(&DATA_STR);
  ATTRIBUTES_STR = rb_const_get(mPankoSerializer, rb_intern("ATTRIBUTES_STR"));
  rb_global_variable(&ATTRIBUTES_STR);
  TYPE_STR = rb_const_get(mPankoSerializer, rb_intern("TYPE_STR"));
  rb_global_variable(&TYPE_STR);
  RELATIONSHIPS_STR = rb_const_get(mPankoSerializer, rb_intern("RELATIONSHIPS_STR"));
  rb_global_variable(&RELATIONSHIPS_STR);
  ID_STR = rb_const_get(mPankoSerializer, rb_intern("ID_STR"));
  rb_global_variable(&ID_STR);
  LINKS_STR = rb_const_get(mPankoSerializer, rb_intern("LINKS_STR"));
  rb_global_variable(&LINKS_STR);

  panko_init_serialization_descriptor(mPanko);
  init_attributes_writer(mPanko);
  panko_init_type_cast(mPanko);
  panko_init_attribute(mPanko);
  panko_init_association(mPanko);
}
