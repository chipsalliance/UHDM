

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

