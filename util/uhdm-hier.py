from uhdm import uhdm
from uhdm import util

import argparse
import os
import sys


def main(uhdm_file, line):
    s = uhdm.Serializer()

    data = s.Restore(uhdm_file)

    if not data:
        print(f"{uhdm_file}: empty design!", file=sys.stderr)
        sys.exit(1)

    result = ""
    for design in data:
        if not util.vpi_is_type(uhdm.vpiDesign, design):
            continue
        print(f"Design name: {uhdm.vpi_get_str(uhdm.vpiName,design)}")
        print(f"Instance tree:")
        print(f"{result}", end="")

        for obj_h in util.vpi_iterate_gen(uhdm.uhdmtopModules, design):

            def inst_visit(obj_h, path):
                res = ""
                objectName = util.vpi_get_str_val(uhdm.vpiName, obj_h)
                defName = util.vpi_get_str_val(uhdm.vpiDefName, obj_h)
                fileName = util.vpi_get_str_val(uhdm.vpiFile, obj_h)

                if objectName:
                    if line:
                        line_info = f" ({defName} {fileName}:{uhdm.vpi_get(uhdm.vpiLineNo, obj_h)}:)"
                    else:
                        line_info = f""
                    print(f"{path}{objectName}{line_info}")
                    path += objectName + "."

                # Recursive tree traversal
                if util.vpi_is_type([uhdm.vpiModule, uhdm.vpiGenScope], obj_h):
                    for sub_h in util.vpi_iterate_gen(uhdm.vpiModule, obj_h):
                        res += inst_visit(sub_h, path)
                    for sub_h in util.vpi_iterate_gen(uhdm.vpiInterface, obj_h):
                        res += inst_visit(sub_h, path)
                if util.vpi_is_type([uhdm.vpiModule, uhdm.vpiGenScope], obj_h):
                    for sub_h in util.vpi_iterate_gen(uhdm.vpiGenScopeArray, obj_h):
                        res += inst_visit(sub_h, path)
                if util.vpi_is_type(uhdm.vpiGenScopeArray, obj_h):
                    for sub_h in util.vpi_iterate_gen(uhdm.vpiGenScope, obj_h):
                        res += inst_visit(sub_h, path)
                return res

            result += inst_visit(obj_h, "")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Reads UHDM binary representation and prints hierarchy tree.",
    )
    parser.add_argument("uhdm_file", help="Path to the UHDM file")
    parser.add_argument("--line", action="store_true", help="Print line info")
    parser.add_argument(
        "--version",
        action="version",
        version=f"{uhdm.UHDM_VERSION_MAJOR}.{uhdm.UHDM_VERSION_MINOR}",
    )

    args = parser.parse_args()
    uhdm_file = args.uhdm_file
    line = args.line

    if not os.path.isfile(uhdm_file):
        print(f"{uhdm_file}: File does not exist!", file=sys.stderr)
        sys.exit(1)

    main(uhdm_file, line)
