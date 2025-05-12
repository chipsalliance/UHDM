from uhdm import uhdm


def vpi_iterate_gen(t, data):
    """
    Generator that iterates over VPI objects using UHDM's vpi_iterate and vpi_scan.

    This function creates a VPI iterator for the given type and data using
    `uhdm.vpi_iterate`, and then yields each object returned by `uhdm.vpi_scan`
    until no more objects are found (i.e., `vpi_scan` returns None).

    Example
    -------
    >>> for obj in vpi_iterate_gen(vpi_object_type, module_handle):
    ...     print(obj)
    """
    vpi_iterator = uhdm.vpi_iterate(t, data)
    yield from iter(lambda: uhdm.vpi_scan(vpi_iterator), None)


def vpi_get_str_val(t, handle):
    """
    Retrieve a string property from a VPI object, returning an empty string if not found.

    This function calls `uhdm.vpi_get_str` with the specified property type `t` and object handle `handle`.
    If the property is not present or the call returns None, an empty string is returned instead.
    """
    s = uhdm.vpi_get_str(t, handle)
    if s is None:
        s = ""
    return s

def vpi_is_type(types, handle):
    """
    Check if a VPI object's type matches the specified type.

    This function retrieves the type of the VPI object referenced by `handle`
    and compares it to the specified type `t`.
    """
    obj_type = uhdm.vpi_get(uhdm.vpiType, handle)
    if isinstance(types, (list, tuple)):
        return obj_type in types
    return obj_type == types

