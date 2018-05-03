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
    RDF rdfOld(system);
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
	ac.sample(frame);
        rdf.sample(frame);
	rdfOld.sampleOld(frame);

	array<double, DIM> position = frame.getAtom(3).getPosition();
	cout << position[0] << " " << position[1] << " " << position[2] << endl;
	cout << endl;
	
	frame.printMolecsInLayer(0);
	frame.printMolecsInLayer(1);
	frame.printMolecsInLayer(2);
	cout << endl;
		
	// frame.printAtomsInLayerCheck(0);
	// frame.printAtomsInLayerCheck(1);
	// frame.printAtomsInLayerCheck(2);
	//doc.sample(frame);

	// clear frame memory
	frame.clearFrame();
      }
    // normalize RDF
    rdf.normalize();
    rdfOld.normalize();
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
    RDFWrite(&rdfOld, "rdfOld");
    RDFWriteLayers(&rdfOld, "rdfOld");
    // RDFMolecWrite(&rdfOld, "rdfmolOld");
    // RDFMolecWriteLayers(&rdfOld, "rdfmolOld");
    ACWriteAtomCounts(&ac, "atoms");
    ACWriteDensity(&ac, "density");
    ACWriteIons(&ac, "ions");
    ACWriteIonsInLayers(&ac, "layers");
    ACWriteIonsInLayersTime(&ac, "numionslayers");
    //ACWriteCVs(&ac, "ionCV");
    //DOCWrite(&doc, "doc");
}
