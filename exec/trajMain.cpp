#include "system.h"
#include "atom.h"
#include "frame.h"
#include "radialDistribution.h"
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
    RDF rdf(system);
    AtomCounter ac(system);
    Frame frame(system); // initialize trajectory frame reader
    cout << "Reading trajectory ..." << endl;
    for (unsigned int frameCounter = 0; frameCounter<system.getNumFrames(); frameCounter++)
      {
	if ( frameCounter % int(ceil(system.getNumFrames()/10.0)) == 0)
	  {
	    cout << frameCounter << endl;
	  }
	// read in step of trajectory
	frame.readStep();

	// sample routines
        //rdf.sample(frame);
	ac.sample(frame);
	//doc.sample(frame);

	// clear frame memory
	frame.clearFrame();
      }
    // normalize RDF
    //rdf.normalize();
    ac.normalize();

    // print to stdout
    //rdf.print();
    //ac.print();

    // print to a file
    //RDFWrite(&rdf, "rdf");
    //RDFWriteLayers(&rdf, "rdf");
    ACWriteDensity(&ac, "density");
    ACWriteIons(&ac, "ions");
    ACWriteIonsInLayers(&ac, "layers");
    //ACWriteCVs(&ac, "ionCV");
    //DOCWrite(&doc, "doc");
}
