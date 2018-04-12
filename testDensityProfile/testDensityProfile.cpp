#include "system.h"
#include "atom.h"
#include "frame.h"
#include "atomCounter.h"
#include <iostream>
#include <array>
using namespace std;

int main(int argc, char** argv)
{
    if(argc != 2)
    {
        cout << "this program takes one argument that is the input file ";
        cout << endl;
        return 1;

    }

    // read inputs
    string inputFile(argv[1]);
    System system(inputFile); // initialize system
    AtomCounter ac(system);
    cout << "number of bins: " << ac.getNumBins() << endl;
    Frame frame(system); // initialize trajectory frame reader
    for (int frameCounter = 0; frameCounter<system.getNumFrames(); frameCounter++)
      {
	frame.readStep();
	array<double, DIM> testPosition = frame.getAtom(0).getPosition();
	cout << testPosition[0] << " " << testPosition[1] << " " << testPosition[2] << endl;
	ac.binElectrolyteCOM(testPosition, 0);
	ac.binAtom(testPosition, 0, 0);
	ac.sample(frame);
	ac.print();
	frame.clearFrame();
	cout << endl;
      }
    // normalize RDF
    // print to a file
}
