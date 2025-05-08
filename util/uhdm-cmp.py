from uhdm import uhdm
from uhdm import util

import argparse
import os
import sys


def main(uhdm_filea, uhdm_fileb):
    if not os.path.isfile(uhdm_filea):
        print(f"{uhdm_filea}: File does not exist!", file=sys.stderr)
        return 1
    sa = uhdm.Serializer()
    ha = sa.Restore(uhdm_filea)
    if not ha:
        print(f"{uhdm_filea}: Failed to load.")
        return 1

    if not os.path.isfile(uhdm_fileb):
        print(f"{uhdm_fileb}: File does not exist!", file=sys.stderr)
        return 1
    sb = uhdm.Serializer()
    hb = sb.Restore(uhdm_fileb)
    if not hb:
        print(f"{uhdm_fileb}: Failed to load.")
        return 1

    if len(ha) != len(hb):
        print(f"Number of designs mismatch.")
        return 1

    for i in range(len(hb)):
        context = uhdm.CompareContext()
        ca = uhdm.UhdmBaseClassFromVpiHandle(ha[i])
        cb = uhdm.UhdmBaseClassFromVpiHandle(hb[i])
        if ca.Compare(cb, context):
            if context.m_failedLhs:
                print(f"LHS: {uhdm.decompile(context.m_failedLhs)}")
            else:
                print(f"LHS: <null>")
            if context.m_failedRhs:
                print(f"RHS: {uhdm.decompile(context.m_failedRhs)}")
            else:
                print(f"RHS: <null>")
            return -1
        return 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description=(
            "Reads input UHDM binary representations of two files and "
            "compares them topographically. "
            f"(Version: {uhdm.UHDM_VERSION_MAJOR}.{uhdm.UHDM_VERSION_MINOR})\n\n"
            "Exits with code:\n"
            "  = 0, if input files are equal\n"
            "  < 0, if input files are not equal\n"
            "  > 0, for any failures"
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("uhdm_filea", help="Path to the first UHDM file")
    parser.add_argument("uhdm_fileb", help="Path to the second UHDM file")

    args = parser.parse_args()
    uhdm_filea = args.uhdm_filea
    uhdm_fileb = args.uhdm_fileb

    rc = main(uhdm_filea, uhdm_fileb)
    sys.exit(rc)

