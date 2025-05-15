from uhdm import uhdm
from uhdm import util

import argparse
import os
import sys


def main(uhdm_file, golden_file, stats, verbose):
    if uhdm_file and not os.path.isfile(uhdm_file):
        print(f"{uhdm_file}: File does not exist!", file=sys.stderr)
        return 1

    if golden_file and not os.path.isfile(golden_file):
        print(f"{golden_file}: File does not exist!", file=sys.stderr)
        return 1

    s = uhdm.Serializer()
    restoredDesigns = s.Restore(uhdm_file)

    if not restoredDesigns:
        print(f"{uhdm_file}: empty design!", file=sys.stderr)
        return 1

    if stats:
        if uhdm_file:
            print(f"{s.GetStats(uhdm_file)}")
        if golden_file:
            print(f"{s.GetStats(golden_file)}")

    print(f"{uhdm_file}: Restored design Pre-Elab: \n{uhdm.visit_designs(restoredDesigns)}")

    if golden_file:
        content = uhdm.visit_designs(restoredDesigns)
        with open(golden_file, "r") as golden_text:
            expected = golden_text.read()

        if expected != content:
            print(f"Dump does not match content of '{golden_file}", file=sys.stderr)
            return 2
        elif verbose:
            print(f"Dump matches '{golden_file}")

    if elab:
        elaboratorContext = uhdm.ElaboratorContext(s, False)
        elaboratorContext.m_elaborator.listenDesigns(restoredDesigns)

        print(f"{uhdm_file}: Restored design Post-Elab: \n{uhdm.visit_designs(restoredDesigns)}")

    return 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Reads UHDM binary representation and prints hierarchy tree.",
    )
    parser.add_argument("uhdm_file", help="Path to the UHDM file")
    parser.add_argument("golden_file", nargs="?", help="Path to the UHDM file")
    parser.add_argument("--elab", action="store_true", help="Print line info")
    parser.add_argument("--stats", action="store_true", help="Print line info")
    parser.add_argument("--verbose", action="store_true", help="Print line info")
    parser.add_argument(
        "--version",
        action="version",
        version=f"{uhdm.UHDM_VERSION_MAJOR}.{uhdm.UHDM_VERSION_MINOR}",
    )

    args = parser.parse_args()
    uhdm_file = args.uhdm_file
    golden_file = args.golden_file
    elab = args.elab
    stats = args.stats
    verbose = args.verbose

    rc = main(uhdm_file, golden_file, stats, verbose)
    sys.exit(rc)

