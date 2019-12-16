all:
	tclsh model_gen.tcl model/uhdm.yaml
	${CC} -g src/main.cpp src/vpi_user.cpp -I. -l stdc++ -o test
	chmod 777 ./test
	./test
