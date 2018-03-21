#include "frame.h"
#include "system.h"
#include "atom.h"
#include <vector>

using namespace std;
class RDF
{
    public:
        RDF();
        /// Constructor
        RDF(System& a_system);
	void sample(Frame& a_frame);
	void normalize();
	void print();
	const unsigned int getNumPairs() const;
	const unsigned int getNumBins() const;
	const double getBinSize() const;
	const double getRDFElement(int i, int j) const;
    private:
        vector<vector< double > > m_rdf;
	vector<int > m_pairCounter;
	System m_system;
	double m_maxDist;
	int m_numBins;
	double m_binSize;
	void binPairDistance(unsigned int a_pair, double a_distance);
	void incrementCounter(unsigned int a_pair);
};

const char* RDFWrite(RDF* a_rdf, const char* a_filename);
