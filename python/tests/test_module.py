import unittest
from uhdm import uhdm
from uhdm import util

class  test_module(unittest.TestCase):

    def test_module(self):
        result = []

        s  = uhdm.Serializer()

        data = uhdm.buildTestDesign(s)
        modit = uhdm.vpi_iterate(uhdm.uhdmallModules,data[0])
        while(True):
            vpiObj = uhdm.vpi_scan(modit)
            if vpiObj is None:
                break
            result.append(uhdm.vpi_get_str(uhdm.vpiName,vpiObj))

        self.assertEqual(set(result),set(["module2","module1"]))

    def test_generator(self):
        result = []

        s = uhdm.Serializer()

        data = uhdm.buildTestDesign(s)

        for vpiObj in util.vpi_iterate_gen(uhdm.uhdmallModules,data[0]):
            result.append(uhdm.vpi_get_str(uhdm.vpiName,vpiObj))

        self.assertEqual(set(result),set(["module2","module1"]))

    def test_ExprEval_size(self):
        s  = uhdm.Serializer()

        result = []
        exprEval = uhdm.ExprEval()

        data = uhdm.buildTestTypedef(s)
        typedefit = uhdm.vpi_iterate(uhdm.vpiTypedef,data[0])
        while(True):
            vpiObj = uhdm.vpi_scan(typedefit)
            if vpiObj is None:
                break
            value, invalidValue = exprEval.size(
                    vpiObj,
                    inst=None,
                    pexpr=None,
                    full=False,
                    muteError=False
                             )

            self.assertEqual(value,32)
            self.assertEqual(invalidValue,False)

    def test_visit_designs(self):
        ref = '''design: 
|uhdmallModules:
\_module_inst: (module1)
  |vpiName:module1
|uhdmallModules:
\_module_inst: (module2)
  |vpiName:module2
'''
        s  = uhdm.Serializer()

        data = uhdm.buildTestDesign(s)
        o = uhdm.visit_designs(data)

        self.assertEqual(o,ref)


if __name__ == '__main__':
    unittest.main()
