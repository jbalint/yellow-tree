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
Fields contains the following properties:
* `name` - field name
* `sig` - signature
* `class` - the class of which this field is a declared member
* `modifiers` - a table of modifiers
 * enum synthetic volatile public private transient final static protected

Field references can be accessed through the `Class.fields` property:
```
yt> print(dump(java.lang.String.fields))
{
["CASE_INSENSITIVE_ORDER"] = jfield_id@0x7fc124002b00 java.lang.String.CASE_INSENSITIVE_ORDER type=Ljava/util/Comparator;,
["serialPersistentFields"] = jfield_id@0x7fc124003030 java.lang.String.serialPersistentFields type=[Ljava/io/ObjectStreamField;,
["HASHING_SEED"] = jfield_id@0x7fc1240012c0 java.lang.String.HASHING_SEED type=I,
["value"] = jfield_id@0x32 java.lang.String.value type=[C,
["hash"] = jfield_id@0x42 java.lang.String.hash type=I,
["serialVersionUID"] = jfield_id@0x7fc124003050 java.lang.String.serialVersionUID type=J,
["hash32"] = jfield_id@0x52 java.lang.String.hash32 type=I,
}
```

Static fields can be accessed directly on the class object:
```
yt> print(java.lang.String.fields.CASE_INSENSITIVE_ORDER.modifiers.static)
true
yt> print(java.lang.String.CASE_INSENSITIVE_ORDER)
jobject@0x7fc1240095d8: java.lang.String$CaseInsensitiveComparator@3bf0d7f5
```

Instance fields can be accessed directly on the object: (field access is not restricted)
```
yt> print(java.lang.String.fields.hash.modifiers.private)
true
yt> x = java.lang.String.new("xyz")
yt> print(x.hash) -- it's 0 until accessed
0
yt> print(x.hashCode())
119193
yt> print(x.hash)
119193
```

## Method API
Methods contain the following properties:
* `name` - method name
* `class` - 
* `sig` - 
* `args`, `ret` - 
* `modifiers` - 
* `line_number_table` -
* `local_variable_table` - 

# Objects
Objects are accessible by reference in a similar manner as in Java.

Examples:
* Classes can be referenced by fully-qualified name:
 * ``> print(java.lang.String)``
 * ``jobject@0x7fcdec0426d8: class java.lang.String``
* Objects can be instantiated by calling the ``new`` method on a class with appropriate constructor arguments:
 * ``uhmm... yeah... it needs some work``
