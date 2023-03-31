import unittest
import uhdm


class  test_module(unittest.TestCase):
    pass

#    def test_module(self):
#        result = []
#
#        s  = uhdm.Serializer()
#
#        data = uhdm.buildTestDesign(s)
#        modit = uhdm.vpi_iterate(uhdm.uhdmallModules,data[0])
#        while(True):
#            vpiObj = uhdm.vpi_scan(modit)
#            if vpiObj is None:
#                break
#            result.append(uhdm.vpi_get_str(uhdm.vpiName,vpiObj))
#
#        self.assertEqual(set(result),set(["module2","module1"]))
#
if __name__ == '__main__':
    unittest.main()
