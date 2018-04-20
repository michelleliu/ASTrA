#ifndef _ATOMCOUNTER_H_
#define _ATOMCOUNTER_H_
#include "frame.h"
#include "system.h"
#include "atom.h"
#include <vector>
#define DENSITYCONV
#define NUM_ION_TYPES 3

using namespace std;
/// A class calculating atom counts and densities.
/**
 * This class stores number of atoms in each region (electrode/liquid), atom number and density profile across the system, and collective variables associated with atom counts.
 */

class AtomCounter
{
 public:
  AtomCounter();
  /// Constructor
  AtomCounter(System& a_system);
  /// Calculates COMs and computes density and number-of-atoms CVs given the current frame.
  void sample(const Frame& a_frame);
  /// Normalizes the density profiles and CVs.
  void normalize();
  /// Prints the density and number of atoms to stdout.
  void print();
  /// Returns the number of bins.
  const int getNumBins() const;
  /// Returns the bin size in length units.
  const double getBinSize() const;
  /// Bin atom.
  void binAtom(array<double, DIM>& a_position, int& a_molecType, int& a_molecMember);
  /// Bin molecule COM.
  void binElectrolyteCOM(array<double, DIM>& a_position, int& a_electrolyteID);
  /// Count number of electrolyte species in given layer of electrode (cathode, anode, liquid)
  void countElectrolyteInLayer(array<double, DIM>& a_position,  vector<array<int, 3> >& a_IonsInLayer, int& a_electrolyteID);
  /// Returns the number of atom types in system.
  const int getNumAtomTypes();
  /// Returns the number of ion types (anion, cation, solvent) in the system.
  const int getNumIonTypes();
  /// Returns the number of layers in the system (typically 3: anode, cathode, liquid).
  const int getNumLayers();
  /// Gets the address of the first element in the vector of binned atom counts.
  double* getACAtomsAddress(int i);
  /// Gets the address of the first element in the vector of binned ion counts.
  double* getACIonsAddress(int i);
  /// Gets the address of the first element in the vector of ion counts per layer.
  double* getACIonsLayersAddress(int i);
  /// Computes the charging mechanisms parameter from Forse 2016 "New perspectives on the charging mechanisms of supercapacitors".
  double computeChargingParam(vector<array<int, NUM_ION_TYPES> >& a_ionsInLayer);

 private:
  System m_system;
  // Center of masses for this timestep.
  vector<array<double, DIM > > m_COMs;
  // Counter of all atoms in bins.
  vector<array<double, MAX_NUM_TYPES > > m_numAtomsProfile;
  // Counter of ion and solvent COMs in bins.
  vector<array<double, NUM_ION_TYPES > > m_numIonsProfile;
  // Counter of ion and solvent COMs in layers (anode, cathode, liquid).
  vector<array<double, NUM_ION_TYPES > > m_avgIonsInLayer;
  // Collective variables for atom and COM counts.
  vector<int > m_excessAnionsInCathode, m_excessCationsInAnode;
  vector<double > m_chargingParam;
  int m_numBins;
  double m_binSize;
  int m_numAtomTypes;
  int m_numLayers;
};

const char* ACWriteDensity(AtomCounter* a_ac, const char* a_filename);
const char* ACWriteIons(AtomCounter* a_ac, const char* a_filename);
const char* ACWriteIonsInLayers(AtomCounter* a_ac, const char* a_filename);
const char* ACWriteCollecVars(AtomCounter* a_ac, const char* a_filename);

#endif
