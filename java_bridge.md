Java Bridge
===========

# Introduction

Java Bridge is a component that allows accessing Java resources (classes, objects, etc) in Lua.
There are three types of resources:
* Object (jobject) - any type of object, even a `java.lang.Class` object
* Method (jmethod_id) - a reference to a method
* Field (jfield_id) - a reference to a field

> Note: All type signatures are in internal form. (``I``, ``[B``, ``Ljava/lang/String;``, etc)

# Fields and Methods
Field and method references are represented in Java by the `java.lang.reflect.Field` and `java.lang.reflect.Method`
classes. They reference a field or method of a class and are not associated with any instance of that class.

## Field API
Fields contains the following members:
* `name`
* `sig` - signature
* `class` - the class which this field is a member
* `modifiers` - a table of modifiers

## Method API

# Objects
Objects are accessible by reference in a similar manner as in Java.

Examples:
* Classes can be referenced by fully-qualified name:
 * ``> print(java.lang.String)``
 * ``jobject@0x7fcdec0426d8: class java.lang.String``
* Objects can be instantiated by calling the ``new`` method on a class with appropriate constructor arguments:
 * ``uhmm... yeah... it needs some work``
