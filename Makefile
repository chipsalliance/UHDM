all:
	tclsh model_gen.tcl model/uhdm.yaml
	${CC} src/main.cpp -I. -o test
