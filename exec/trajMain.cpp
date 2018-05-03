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

    // Skip frames
    frame.skipSteps();

    cout << "Analyzing zero-P, zero-Q run of " << system.getNumZPFrames() << " steps..." << endl;

    // Read zero-potential, zero-charge frames
    for (unsigned int frameCounter = 0; frameCounter<system.getNumZPFrames(); frameCounter++)
      {
	frame.readZPStep();

	if (frame.getZPStepNum() % int(ceil(system.getNumTotalFrames()/10.0)) == 0)
	  {
	    cout << frame.getZPStepNum() << endl;
	  }

	//ac.sampleZP(frame);
	//rdf.sampleZP(frame);
      	frame.clearFrame();
      }
    ac.normalizeZP();
    
    // Skip frames (potential or charge turned out)
    frame.skipSteps();

    cout << "Analyzing constant-P or -Q run of " << system.getNumFrames() << " steps..." << endl;

    // Read constant-potential or constant-charge frames
    for (unsigned int frameCounter = 0; frameCounter<system.getNumFrames(); frameCounter++)
      {
	// read in step of trajectory
	frame.readStep();

	if ( frame.getStepNum() % int(ceil(system.getNumTotalFrames()/10.0)) == 0)
	  {
	    cout << frame.getStepNum() << endl;
	  }
	
	// sample routines
	ac.sample(frame);
        rdf.sample(frame);
	//doc.sample(frame);

	// clear frame memory
	frame.clearFrame();
      }
    // normalize RDF
    rdf.normalize();
    //ac.printDensity();
    ac.normalize();

    // print to stdout
    //rdf.print();
    //ac.print();

    // print to a file
    RDFWrite(&rdf, "rdf");
    RDFWriteLayers(&rdf, "rdf");
    RDFMolecWrite(&rdf, "rdfmol");
    RDFMolecWriteLayers(&rdf, "rdfmol");
    ACWriteAtomCounts(&ac, "atoms");
    ACWriteDensity(&ac, "density");
    ACWriteIons(&ac, "ions");
    ACWriteIonsInLayers(&ac, "layers");
    ACWriteIonsInLayersTime(&ac, "numionslayers");
    //ACWriteCVs(&ac, "ionCV");
    //DOCWrite(&doc, "doc");
}
