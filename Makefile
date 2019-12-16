GENERATED_HEADERS=headers/module.h headers/design.h
GENERATED_SOURCE=src/vpi_user.cpp

all: generate-api run-test

generate-api: $(GENERATED_SOURCE)

$(GENERATED_SOURCE): model_gen.tcl model/uhdm.yaml
	tclsh ./model_gen.tcl model/uhdm.yaml

unittest: src/vpi_user.cpp
	$(CXX) -g src/main.cpp src/vpi_user.cpp -I. -o $@

run-test: unittest
	./unittest

clean:
	rm -f $(GENERATED_SOURCE) $(GENERATED_HEADERS) unittest
