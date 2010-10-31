all: dro2imf

dro2imf: dro2imf.cpp
	$(CXX) -o $@ $<
