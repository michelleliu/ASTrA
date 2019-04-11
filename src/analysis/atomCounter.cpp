#include "frame.h"
#include "system.h"
#include "atom.h"
#include <vector>
#include "atomCounter.h"
#include <assert.h>
#include <iostream>
#include "writer.h"

using namespace std;

AtomCounter::AtomCounter()
{
};

AtomCounter::AtomCounter(System& a_system)
{
  m_system = a_system;
  m_saveFrameEvery = 1;
  m_numSavedFrames = ceil(m_system.getNumTotalFrames() / m_saveFrameEvery) ;
  m_zLo = m_system.getZLo();
  m_binSize = 0.05; //!! FIXME make this choose a bin size automatically based on cell size?
  m_numBins = ceil(m_system.getBoxDim(2) / m_binSize );
  m_numLayers = m_system.getNumLayers();
  m_maxAtomCharge = 0.04; // [e]
  m_numBinsAtomCharge = 800; // for positive and negative
  m_binSizeAtomCharge = m_maxAtomCharge*2.0 / (1.0*m_numBinsAtomCharge) ;
  m_numElectrolyteSpecies = m_system.getNumElectrolyteSpecies() ;
  // resize vectors
  m_COMs.resize(m_system.getNumMolecules());

  m_numAtomsProfile.resize(m_numBins);
  m_densityProfile.resize(m_numBins);
  m_electrodeChargeProfile.resize(m_numBins);
  m_numIonsProfile.resize(m_numBins);
  m_avgIonsInLayer.resize( m_numLayers );
  m_numIonsInLayerTime.resize(m_numSavedFrames);

  m_elecSliceCharge.resize(m_numSavedFrames);
  m_elecSliceNumIons.resize(m_numSavedFrames);

  m_layerBounds.resize(m_numLayers);
  for (int i=0; i<m_numLayers; i++)
  {
    m_layerBounds[i] = m_system.getLayerUpperBound(i);
  }
  m_sliceHeight = (m_layerBounds[0] - 3.0)/(1.0*NUM_ELEC_SLICES);

  for (int i=0; i<m_numSavedFrames; i++)
  {
    m_numIonsInLayerTime[i].resize( m_numLayers );
    m_elecSliceCharge[i].resize( NUM_ELEC_SLICES );
    m_elecSliceNumIons[i].resize( NUM_ELEC_SLICES*m_numElectrolyteSpecies );
  }
  m_numZPAtomsProfile.resize(m_numBins);
  m_ZPdensityProfile.resize(m_numBins);
  m_numZPIonsProfile.resize(m_numBins);
  m_avgZPIonsInLayer.resize( m_numLayers );

  m_numAtomTypes=m_system.getNumAtomTypes();
  m_chargingParam.resize( m_system.getNumFrames() );

  m_elecAtomChargeHist.resize( m_numBinsAtomCharge );
  for (int i=0; i<m_numBinsAtomCharge; i++)
  {
    m_elecAtomChargeHist[i].resize( m_system.getNumEleGrps() );
  }
};

void AtomCounter::sample(Frame& a_frame)
{
  // setup
  int molecIndex = 0;
  int atomIndex = 0;
  int isElectrolyte = 0;
  int isElectrode = 0;
  array<double, DIM > com;
  array<double, DIM > x0;
  vector<array<int, NUM_ION_TYPES> > currentIonsInLayer;
  currentIonsInLayer.resize(m_numLayers);
  vector<array<int, NUM_ION_TYPES> > currentIonsInAnodeSlice;
  vector<array<int, NUM_ION_TYPES> > currentIonsInCathodeSlice;
  vector<double > cathodeChargeSlice;
  vector<double > anodeChargeSlice;
  cathodeChargeSlice.resize(NUM_ELEC_SLICES);
  anodeChargeSlice.resize(NUM_ELEC_SLICES);
  currentIonsInAnodeSlice.resize(NUM_ELEC_SLICES);
  currentIonsInCathodeSlice.resize(NUM_ELEC_SLICES);
  // Starting loop...
  // For each molecule type
  for (int i=0; i<m_system.getNumMolecTypes(); i++)
  {
    int numMembers = m_system.getNumMembersMolec(i);
    int* electrolyteID = new int;
    int* electrodeID = new int;
    // Detect whether molecule is electrolyte or not (affects binning routines later on)
    if (m_system.isElectrolyte(i, electrolyteID))
    {
      isElectrolyte = 1;
    }
    else
    {
      isElectrolyte = 0;
    }
    if (m_system.isElectrode(i, electrodeID))
    {
      isElectrode = 1;
    }
    else
    {
      isElectrode = 0;
    }
    // Get masses of molecule members, and molecule's total mass, to calculate COM
    array<double , MAX_MEMBERS_PER_MOLEC > masses = m_system.getMassesOfType(i);
    double totalMass = 0;
    for (int k=0; k < numMembers; k++)
    {
      totalMass += masses[k];
    }
    // For each molecule of type i
    for (int j=0; j < m_system.getNumMolecsOfType(i); j++)
    {
      com.fill(0); // fill with zeros, otherwise will keep same data as before
      x0.fill(0);
      // For each molecule member
      for (int k=0; k < numMembers; k++)
      {
        // Get position of atom
        array<double, DIM> position = a_frame.getAtom(atomIndex).getPosition();
        // Bin atom by type and add to density and charge profile
        binAtom(a_frame, atomIndex, position, i, k, masses[k], isElectrolyte, isElectrode);
        // Compute center of mass
        if (k == 0)
        {
          for (int l=0; l < DIM; l++)
          {
            x0[l] = position[l];
            com[l] += position[l]*masses[k];
          }
        }
        else if (k > 0)
        {
          for (int l=0; l < DIM; l++)
          {
            double dx = position[l] - x0[l];
            if (m_system.isPeriodic(l))
            {
              double dim = m_system.getBoxDim(l);
              dx -= round(dx/dim) * dim;
            }
            com[l] += (x0[l]+dx)*masses[k];
          }
        }
        // Sample slices FIXME: needs to be streamlined
        if (isElectrolyte)
        {
          double z;
          if (m_system.isZSymmetrized())
          {
            z = position[2] - m_system.getZLo();
          }
          else
          {
            z = position[2];
          }
          if (z < m_layerBounds[0])
          {
            double h = z - 3.0;
            int slice = floor(h/m_sliceHeight);
            assert(slice < 2);
            if (m_system.isCathodeLower())
            {
              currentIonsInCathodeSlice[slice][*electrolyteID] += 1;
            }
            else
            {
              currentIonsInAnodeSlice[slice][*electrolyteID] += 1;
            }
          }
          else if (z > m_layerBounds[1])
          {
            double h = z - m_layerBounds[1];
            int slice = floor(h/m_sliceHeight);
            assert(slice < 2);
            if (m_system.isCathodeLower())
            {
              currentIonsInAnodeSlice[slice][*electrolyteID] += 1;
            }
            else
            {
              currentIonsInCathodeSlice[slice][*electrolyteID] += 1;
            }
          }
        }
        if (isElectrode)
        {
          double q = a_frame.getAtom(atomIndex).getCharge();
          double z;
          if (m_system.isZSymmetrized())
          {
            z = position[2] - m_system.getZLo();
          }
          else
          {
            z = position[2];
          }
          if (*electrodeID == 0)
          {
            if(m_system.isCathodeLower())
            {
              double h = z - 3.0;
              int slice = floor(h/m_sliceHeight);
              assert(slice < 2);
              cathodeChargeSlice[slice] += q;
            }
            else
            {
              double h = z - m_layerBounds[1];
              int slice = floor(h/m_sliceHeight);
              assert(slice < 2);
              cathodeChargeSlice[slice] += q;
            }
          }
          else if (*electrodeID == 1)
          {
            if(m_system.isCathodeLower())
            {
              double h = z - m_layerBounds[1];
              int slice = floor(h/m_sliceHeight);
              assert(slice < 2);
              anodeChargeSlice[slice] += q;
            }
            else
            {
              double h = z - 3.0;
              int slice = floor(h/m_sliceHeight);
              assert(slice < 2);
              anodeChargeSlice[slice] += q;
            }
          }
        }
        atomIndex++;
      }
      for (int l=0; l<DIM; l++)
      {
        com[l] /= totalMass;
      }
      m_COMs[molecIndex]=com;
      binElectrolyteCOM(a_frame, molecIndex, com, i, currentIonsInLayer, *electrolyteID, isElectrolyte); // different for ZP and skip
      molecIndex++;
    }
    delete electrolyteID;
    delete electrodeID;
  }
  for (int i=0; i<m_numLayers; i++)
  {
    for (int j=0; j<m_numElectrolyteSpecies; j++)
    {
      // compute average number of ions j in layer i
      m_avgIonsInLayer[i][j] += currentIonsInLayer[i][j]; // different for ZP and skip
    }
  }
  // If this frame is to be saved, write number of ions in each layer
  if ( (a_frame.getTotalStepNum() % m_saveFrameEvery == 0) || ( a_frame.getTotalStepNum() == m_system.getNumTotalFrames() ) )
  {
    // index into numIonsInLayerTime ( frame number from beginning of trajectory, w/o "skipping" any steps )
    int timeIndex = (ceil)( ((double)a_frame.getTotalStepNum()) / ((double)m_saveFrameEvery) );
    for (int i=0; i<NUM_ELEC_SLICES; i++)
    {
      m_elecSliceCharge[timeIndex][i][0] += cathodeChargeSlice[i];
      m_elecSliceCharge[timeIndex][i][1] += anodeChargeSlice[i];
      for (int j=0; j<m_numElectrolyteSpecies; j++)
      {
        m_elecSliceNumIons[timeIndex][i*m_numElectrolyteSpecies+j][0] += currentIonsInCathodeSlice[i][j];
        m_elecSliceNumIons[timeIndex][i*m_numElectrolyteSpecies+j][1] += currentIonsInAnodeSlice[i][j];
      }
    }
    for (int i=0; i<m_numLayers; i++)
    {
      for (int j=0; j<m_numElectrolyteSpecies; j++)
      {
        m_numIonsInLayerTime[timeIndex][i][j] = currentIonsInLayer[i][j]; // store number of ions in layer at this frame
      }
    }
  }
  a_frame.setCOMs(m_COMs);
  // compute charging parameter
  m_chargingParam[a_frame.getStepNum()] = computeChargingParam(currentIonsInLayer); // only when sampling production runs
}

void AtomCounter::sampleZP(Frame& a_frame)
{

  // setup
  int molecIndex = 0;
  int atomIndex = 0;
  int isElectrolyte = 0;
  int isElectrode = 0;
  array<double, DIM > com;
  array<double, DIM > x0;
  vector<array<int, NUM_ION_TYPES> > currentIonsInLayer;
  currentIonsInLayer.resize(m_numLayers);
  vector<array<int, NUM_ION_TYPES> > currentIonsInAnodeSlice;
  vector<array<int, NUM_ION_TYPES> > currentIonsInCathodeSlice;
  vector<double > cathodeChargeSlice;
  vector<double > anodeChargeSlice;
  cathodeChargeSlice.resize(NUM_ELEC_SLICES);
  anodeChargeSlice.resize(NUM_ELEC_SLICES);
  currentIonsInAnodeSlice.resize(NUM_ELEC_SLICES);
  currentIonsInCathodeSlice.resize(NUM_ELEC_SLICES);
  // Starting loop...
  // For each molecule type
  for (int i=0; i<m_system.getNumMolecTypes(); i++)
  {
    int numMembers = m_system.getNumMembersMolec(i);
    int* electrolyteID = new int;
    int* electrodeID = new int;
    // Detect whether molecule is electrolyte or not (affects binning routines later on)
    if (m_system.isElectrolyte(i, electrolyteID))
    {
      isElectrolyte = 1;
    }
    else
    {
      isElectrolyte = 0;
    }
    if (m_system.isElectrode(i, electrodeID))
    {
      isElectrode = 1;
    }
    else
    {
      isElectrode = 0;
    }
    // Get masses of molecule members, and molecule's total mass, to calculate COM
    array<double , MAX_MEMBERS_PER_MOLEC > masses = m_system.getMassesOfType(i);
    double totalMass = 0;
    for (int k=0; k < numMembers; k++)
    {
      totalMass += masses[k];
    }
    // For each molecule of type
    for (int j=0; j < m_system.getNumMolecsOfType(i); j++)
    {
      com.fill(0); // fill with zeros, otherwise will keep same data as before
      x0.fill(0);
      // For each molecule member
      for (int k=0; k < numMembers; k++)
      {
        // Get position of atom
        array<double, DIM> position = a_frame.getAtom(atomIndex).getPosition();
        // Bin atom by type and add to density
        binZPAtom(a_frame, atomIndex, position, i, k, masses[k], isElectrolyte);
        // Compute center of mass
        if (k == 0)
        {
          for (int l=0; l < DIM; l++)
          {
            x0[l] = position[l];
            com[l] += position[l]*masses[k];
          }
        }
        else if (k > 0)
        {
          for (int l=0; l < DIM; l++)
          {
            double dx = position[l] - x0[l];
            if (m_system.isPeriodic(l))
            {
              double dim = m_system.getBoxDim(l);
              dx -= round(dx/dim) * dim;
            }
            com[l] += (x0[l]+dx)*masses[k];
          }
        }
        // Sample slices FIXME: needs to be streamlined
        if (isElectrolyte)
        {
          double z;
          if (m_system.isZSymmetrized())
          {
            z = position[2] - m_system.getZLo();
          }
          else
          {
            z = position[2];
          }
          if (z < m_layerBounds[0])
          {
            double h = z - 3.0;
            int slice = floor(h/m_sliceHeight);
            assert(slice < 2);
            if (m_system.isCathodeLower())
            {
              currentIonsInCathodeSlice[slice][*electrolyteID] += 1;
            }
            else
            {
              currentIonsInAnodeSlice[slice][*electrolyteID] += 1;
            }
          }
          else if (z > m_layerBounds[1])
          {
            double h = z - m_layerBounds[1];
            int slice = floor(h/m_sliceHeight);
            assert(slice < 2);
            if (m_system.isCathodeLower())
            {
              currentIonsInAnodeSlice[slice][*electrolyteID] += 1;
            }
            else
            {
              currentIonsInCathodeSlice[slice][*electrolyteID] += 1;
            }
          }
        }
        if (isElectrode)
        {
          double q = a_frame.getAtom(atomIndex).getCharge();
          double z;
          if (m_system.isZSymmetrized())
          {
            z = position[2] - m_system.getZLo();
          }
          else
          {
            z = position[2];
          }
          if (*electrodeID == 0)
          {
            if(m_system.isCathodeLower())
            {
              double h = z - 3.0;
              int slice = floor(h/m_sliceHeight);
              assert(slice < 2);
              cathodeChargeSlice[slice] += q;
            }
            else
            {
              double h = z - m_layerBounds[1];
              int slice = floor(h/m_sliceHeight);
              assert(slice < 2);
              cathodeChargeSlice[slice] += q;
            }
          }
          else if (*electrodeID == 1)
          {
            if(m_system.isCathodeLower())
            {
              double h = z - m_layerBounds[1];
              int slice = floor(h/m_sliceHeight);
              assert(slice < 2);
              anodeChargeSlice[slice] += q;
            }
            else
            {
              double h = z - 3.0;
              int slice = floor(h/m_sliceHeight);
              assert(slice < 2);
              anodeChargeSlice[slice] += q;
            }
          }
        }
        atomIndex++;
      }
      for (int l=0; l<DIM; l++)
      {
        com[l] /= totalMass;
      }
      m_COMs[molecIndex]=com;
      binZPElectrolyteCOM(a_frame, molecIndex, com, i, currentIonsInLayer, *electrolyteID, isElectrolyte); // different for ZP and skip
      // (during ZP, calculates initial # of ions for charging mechanism param)
      molecIndex++;
    }
    delete electrolyteID;
    delete electrodeID;
  }
  for (int i=0; i<m_numLayers; i++)
  {
    for (int j=0; j<m_numElectrolyteSpecies; j++)
    {
      m_avgZPIonsInLayer[i][j] += currentIonsInLayer[i][j]; // different for ZP and skip
    }
  }
  if ( (a_frame.getTotalStepNum() % m_saveFrameEvery == 0) || ( a_frame.getTotalStepNum() == m_system.getNumTotalFrames() ) )
  {
    int timeIndex = (ceil)( ((double)a_frame.getTotalStepNum()) / ((double)m_saveFrameEvery) );
    for (int i=0; i<NUM_ELEC_SLICES; i++)
    {
      m_elecSliceCharge[timeIndex][i][0] += cathodeChargeSlice[i];
      m_elecSliceCharge[timeIndex][i][1] += anodeChargeSlice[i];
      for (int j=0; j<m_numElectrolyteSpecies; j++)
      {
        m_elecSliceNumIons[timeIndex][i*m_numElectrolyteSpecies+j][0] += currentIonsInCathodeSlice[i][j];
        m_elecSliceNumIons[timeIndex][i*m_numElectrolyteSpecies+j][1] += currentIonsInAnodeSlice[i][j];
      }
    }
    for (int i=0; i<m_numLayers; i++)
    {
      for (int j=0; j<m_numElectrolyteSpecies; j++)
      {
        m_numIonsInLayerTime[timeIndex][i][j] = currentIonsInLayer[i][j]; // store number of ions in layer at this frame
      }
    }
  }
  a_frame.setCOMs(m_COMs);
}

void AtomCounter::sampleSkip(Frame& a_frame)
{

  // setup
  int molecIndex = 0;
  int atomIndex = 0;
  int isElectrolyte = 0;
  int isElectrode = 0;
  array<double, DIM > com;
  array<double, DIM > x0;
  vector<array<int, NUM_ION_TYPES> > currentIonsInLayer;
  currentIonsInLayer.resize(m_numLayers);
  vector<array<int, NUM_ION_TYPES> > currentIonsInAnodeSlice;
  vector<array<int, NUM_ION_TYPES> > currentIonsInCathodeSlice;
  vector<double > cathodeChargeSlice;
  vector<double > anodeChargeSlice;
  cathodeChargeSlice.resize(NUM_ELEC_SLICES);
  anodeChargeSlice.resize(NUM_ELEC_SLICES);
  currentIonsInAnodeSlice.resize(NUM_ELEC_SLICES);
  currentIonsInCathodeSlice.resize(NUM_ELEC_SLICES);
  // Starting loop...
  // For each molecule type
  for (int i=0; i<m_system.getNumMolecTypes(); i++)
  {
    int numMembers = m_system.getNumMembersMolec(i);
    int* electrolyteID = new int;
    int* electrodeID = new int;
    // Detect whether molecule is electrolyte or not (affects binning routines later on)
    if (m_system.isElectrolyte(i, electrolyteID))
    {
      isElectrolyte = 1;
    }
    else
    {
      isElectrolyte = 0;
    }
    if (m_system.isElectrode(i, electrodeID))
    {
      isElectrode = 1;
    }
    else
    {
      isElectrode = 0;
    }
    // Get masses of molecule members, and molecule's total mass, to calculate COM
    array<double , MAX_MEMBERS_PER_MOLEC > masses = m_system.getMassesOfType(i);
    double totalMass = 0;
    for (int k=0; k < numMembers; k++)
    {
      totalMass += masses[k];
    }
    // For each molecule of type
    for (int j=0; j < m_system.getNumMolecsOfType(i); j++)
    {
      com.fill(0); // fill with zeros, otherwise will keep same data as before
      x0.fill(0);
      // For each molecule member
      for (int k=0; k < numMembers; k++)
      {
        // Get position of atom
        array<double, DIM> position = a_frame.getAtom(atomIndex).getPosition();

        // Compute center of mass
        if (k == 0)
        {
          for (int l=0; l < DIM; l++)
          {
            x0[l] = position[l];
            com[l] += position[l]*masses[k];
          }
        }
        else if (k > 0)
        {
          for (int l=0; l < DIM; l++)
          {
            double dx = position[l] - x0[l];
            if (m_system.isPeriodic(l))
            {
              double dim = m_system.getBoxDim(l);
              dx -= round(dx/dim) * dim;
            }
            com[l] += (x0[l]+dx)*masses[k];
          }
        }
        // Sample slices FIXME: needs to be streamlined
        if (isElectrolyte)
        {
          double z;
          if (m_system.isZSymmetrized())
          {
            z = position[2] - m_system.getZLo();
          }
          else
          {
            z = position[2];
          }
          if (z < m_layerBounds[0])
          {
            double h = z - 3.0;
            int slice = floor(h/m_sliceHeight);
            assert(slice < 2);
            if (m_system.isCathodeLower())
            {
              currentIonsInCathodeSlice[slice][*electrolyteID] += 1;
            }
            else
            {
              currentIonsInAnodeSlice[slice][*electrolyteID] += 1;
            }
          }
          else if (z > m_layerBounds[1])
          {
            double h = z - m_layerBounds[1];
            int slice = floor(h/m_sliceHeight);
            assert(slice < 2);
            if (m_system.isCathodeLower())
            {
              currentIonsInAnodeSlice[slice][*electrolyteID] += 1;
            }
            else
            {
              currentIonsInCathodeSlice[slice][*electrolyteID] += 1;
            }
          }
        }
        if (isElectrode)
        {
          double q = a_frame.getAtom(atomIndex).getCharge();
          double z;
          if (m_system.isZSymmetrized())
          {
            z = position[2] - m_system.getZLo();
          }
          else
          {
            z = position[2];
          }
          if (*electrodeID == 0)
          {
            if(m_system.isCathodeLower())
            {
              double h = z - 3.0;
              int slice = floor(h/m_sliceHeight);
              assert(slice < 2);
              cathodeChargeSlice[slice] += q;
            }
            else
            {
              double h = z - m_layerBounds[1];
              int slice = floor(h/m_sliceHeight);
              assert(slice < 2);
              cathodeChargeSlice[slice] += q;
            }
          }
          else if (*electrodeID == 1)
          {
            if(m_system.isCathodeLower())
            {
              double h = z - m_layerBounds[1];
              int slice = floor(h/m_sliceHeight);
              assert(slice < 2);
              anodeChargeSlice[slice] += q;
            }
            else
            {
              double h = z - 3.0;
              int slice = floor(h/m_sliceHeight);
              assert(slice < 2);
              anodeChargeSlice[slice] += q;
            }
          }
        }
        atomIndex++;
      }
      for (int l=0; l<DIM; l++)
      {
        com[l] /= totalMass;
      }
      m_COMs[molecIndex]=com;
      binSkipElectrolyteCOM(a_frame, molecIndex, com, i, currentIonsInLayer, *electrolyteID, isElectrolyte); // different for ZP and skip
      molecIndex++;
    }
    delete electrolyteID;
    delete electrodeID;
  }

  if ( (a_frame.getTotalStepNum() % m_saveFrameEvery == 0) || ( a_frame.getTotalStepNum() == m_system.getNumTotalFrames() ) )
  {
    int timeIndex = (ceil)( ((double)a_frame.getTotalStepNum()) / ((double)m_saveFrameEvery) );
    for (int i=0; i<NUM_ELEC_SLICES; i++)
    {
      m_elecSliceCharge[timeIndex][i][0] += cathodeChargeSlice[i];
      m_elecSliceCharge[timeIndex][i][1] += anodeChargeSlice[i];
      for (int j=0; j<m_numElectrolyteSpecies; j++)
      {
        m_elecSliceNumIons[timeIndex][i*m_numElectrolyteSpecies+j][0] += currentIonsInCathodeSlice[i][j];
        m_elecSliceNumIons[timeIndex][i*m_numElectrolyteSpecies+j][1] += currentIonsInAnodeSlice[i][j];
      }
    }
    for (int i=0; i<m_numLayers; i++)
    {
      for (int j=0; j<m_numElectrolyteSpecies; j++)
      {
        m_numIonsInLayerTime[timeIndex][i][j] = currentIonsInLayer[i][j]; // during skip steps, just for tracking
      }
    }
  }
  a_frame.setCOMs(m_COMs);

}


void AtomCounter::binZPAtom(Frame& a_frame,  int& a_atomIndex, array<double, DIM>& a_position, int& a_molecType, int& a_molecMember, double& a_mass, int& a_isElectrolyte)
{
  double pos_z = a_position[2];
  int bin = floor(pos_z / m_binSize);
  int atomType = m_system.getAtomType(a_molecType, a_molecMember);
#ifdef DEBUG
  assert ( atomType < m_system.getNumAtomTypes() ) ;
#endif
  // Increment number of atoms in bin
  m_numZPAtomsProfile[bin][atomType]++;
  if (a_isElectrolyte)
  {
    // Increment density of electrolyte in bin
    m_ZPdensityProfile[bin] += a_mass;
  }
  unsigned int layer = m_system.getLayer(a_position);
  a_frame.assignZPAtomToLayer(a_atomIndex, atomType, layer);
}

void AtomCounter::binZPElectrolyteCOM(Frame& a_frame, int& a_molecIndex, array<double, DIM>& a_position, int& a_molecType, vector<array<int, NUM_ION_TYPES> >& a_currentIonsInLayer, int& a_electrolyteID, int& a_isElectrolyte)
{
  // increments a_currentIonsInLayer and m_numZPIonsInProfile
  unsigned int layer = m_system.getLayer(a_position);
  a_frame.assignZPIonToLayer(a_molecIndex, a_molecType, layer);
  if(a_isElectrolyte)
  {
#ifdef DEBUG
    assert(a_electrolyteID > -1);
    assert(a_electrolyteID < 3);
#endif
    double pos_z = a_position[2];
    int bin = floor(pos_z / m_binSize);
#ifdef DEBUG
    assert( bin < m_numBins );
#endif
    m_numZPIonsProfile[bin][a_electrolyteID]++;
    // Figure out what layer electrolyte molecule is in
    // Increment count of molecule inside layer
    a_currentIonsInLayer[layer][a_electrolyteID]++;
  }
}

void AtomCounter::binSkipElectrolyteCOM(Frame& a_frame, int& a_molecIndex, array<double, DIM>& a_position, int& a_molecType, vector<array<int, NUM_ION_TYPES> >& a_currentIonsInLayer, int& a_electrolyteID, int& a_isElectrolyte)
{
  // inly increments a_currentIonsInLayer
  unsigned int layer = m_system.getLayer(a_position);
  if(a_isElectrolyte)
  {
    // Figure out what layer electrolyte molecule is in
    // Increment count of molecule inside layer
    a_currentIonsInLayer[layer][a_electrolyteID]++;
  }
}
void AtomCounter::binAtom(Frame& a_frame,  int& a_atomIndex, array<double, DIM>& a_position, int& a_molecType, int& a_molecMember, double& a_mass, int& a_isElectrolyte, int& a_isElectrode)
{
  double pos_z = a_position[2] - m_zLo;
  int bin = floor(pos_z / m_binSize);
  int atomType = m_system.getAtomType(a_molecType, a_molecMember);
#ifdef DEBUG
  assert ( atomType < m_system.getNumAtomTypes() ) ;
#endif
  // Increment number of atoms in bin
  m_numAtomsProfile[bin][atomType]++;
  if (a_isElectrolyte)
  {
    // Increment density of electrolyte in bin
    m_densityProfile[bin] += a_mass;
  }
  if (a_isElectrode)
  {
    // Increment electrode charge in bin
    double charge = a_frame.getAtom(a_atomIndex).getCharge();
    m_electrodeChargeProfile[bin] += charge;
    int binAtomCharge = floor(charge / m_binSizeAtomCharge) + m_numBinsAtomCharge/2; // accounts for negative charges
    if (binAtomCharge < 0)
    {
      binAtomCharge = 0;
    }
    else if (binAtomCharge > m_numBinsAtomCharge-1)
    {
      binAtomCharge = m_numBinsAtomCharge-1;
    }

    // return iterator to all subgroupings that this electrode atom belongs to
    // for now just anode and cathode
    vector<unsigned int> thisElecAtomGroups = m_system.getAtomIndexToGroup(a_atomIndex-m_system.getElectrodeAtomIndexOffset());
    vector<unsigned int>::iterator atomGrpItr = thisElecAtomGroups.begin();
    while (atomGrpItr != thisElecAtomGroups.end())
    {
      // bin charge
      m_elecAtomChargeHist[binAtomCharge][*atomGrpItr] += 1.0;
      atomGrpItr++;
    }

  }
  unsigned int layer = m_system.getLayer(a_position);
  a_frame.assignAtomToLayer(a_atomIndex, atomType, layer);
}

void AtomCounter::binElectrolyteCOM(Frame& a_frame, int& a_molecIndex, array<double, DIM>& a_position, int& a_molecType, vector<array<int, NUM_ION_TYPES> >& a_currentIonsInLayer, int& a_electrolyteID, int& a_isElectrolyte)
{
  unsigned int layer = m_system.getLayer(a_position);
  a_frame.assignIonToLayer(a_molecIndex, a_molecType, layer);
  if(a_isElectrolyte)
  {
#ifdef DEBUG
    assert(a_electrolyteID > -1);
    assert(a_electrolyteID < 3);
#endif
    double pos_z = a_position[2] - m_zLo;
    int bin = floor(pos_z / m_binSize);
#ifdef DEBUG
    assert( bin < m_numBins );
#endif
    m_numIonsProfile[bin][a_electrolyteID]++;
    // Figure out what layer electrolyte molecule is in
    // Increment count of molecule inside layer
    a_currentIonsInLayer[layer][a_electrolyteID]++;
  }
}

const int AtomCounter::getNumAtomTypes()
{
  return m_numAtomTypes;
}

const int AtomCounter::getNumIonTypes()
{
  return NUM_ION_TYPES;
}

const int AtomCounter::getNumLayers() const
{
  return m_numLayers;
}

void AtomCounter::normalizeZP()
{
  // Normalize average ions in layer:
  double avogadro = 6.022140857;
  // Divide by number of frames read
  double numFrames = m_system.getNumZPFrames();
  // Convert density to g/ml
  double normDensity = numFrames * m_binSize * m_system.getBoxDim(0) * m_system.getBoxDim(1) * 10. / avogadro;
  for (int i=0; i<m_numLayers; i++)
  {
    for (int j=0; j<m_numElectrolyteSpecies; j++)
    {
      m_avgZPIonsInLayer[i][j] /= numFrames;
    }
  }
  for (int i=0; i<m_numBins; i++)
  {
    for (int j=0; j<m_system.getNumAtomTypes(); j++)
    {
      m_numZPAtomsProfile[i][j] /= numFrames;
    }
    for (int j=0; j<getNumIonTypes(); j++)
    {
      m_numZPIonsProfile[i][j] /= numFrames;
    }
    m_ZPdensityProfile[i] /= normDensity ;
  }
}

void AtomCounter::normalize()
{
  // Normalize average ions in layer:
  double avogadro = 6.022140857;
  // Divide by number of frames read
  double numFrames = m_system.getNumFrames();
  // Convert density to g/ml
  double normDensity = numFrames * m_binSize * m_system.getBoxDim(0) * m_system.getBoxDim(1) * avogadro / 10.;
  for (int i=0; i<m_numLayers; i++)
  {
    for (int j=0; j<m_numElectrolyteSpecies; j++)
    {
      m_avgIonsInLayer[i][j] /= numFrames;
    }
  }
  for (int i=0; i<m_numBins; i++)
  {
    for (int j=0; j<m_system.getNumAtomTypes(); j++)
    {
      m_numAtomsProfile[i][j] /= numFrames;
    }
    for (int j=0; j<getNumIonTypes(); j++)
    {
      m_numIonsProfile[i][j] /= numFrames;
    }
    m_densityProfile[i] /= normDensity ;
    m_electrodeChargeProfile[i] /= numFrames;
  }
  // normalize histogram of charges
  // for number of electrode atom groups, divide by number of prod timesteps and number of atoms in group
  for (int i=0; i<m_system.getNumEleGrps(); i++)
  {
    double normElecAtomChargeHist = numFrames * m_system.getAtomGroupToIndex(i).size();
    // for number of bins
    for (int j=0; j<m_numBinsAtomCharge; j++)
    {
      m_elecAtomChargeHist[j][i] /= normElecAtomChargeHist;
    }
  }

}

void AtomCounter::print()
{
  for (int i=0; i<m_numBins; i++)
  {
    cout << i*getBinSize();
    for (int j=0; j<m_system.getNumAtomTypes(); j++)
    {
      cout << " " << m_numAtomsProfile[i][j];
    }
    for (int j=0; j<NUM_ION_TYPES; j++)
    {
      cout << " " << m_numIonsProfile[i][j];
    }
    cout << endl;
  }
  cout << endl;
  for (int i=0; i<m_numLayers; i++)
  {
    for (int j=0; j<m_numElectrolyteSpecies; j++)
    {
      cout << m_avgIonsInLayer[i][j] << " ";
    }
    cout << endl;
  }
  cout << endl;
  for (int i=0; i<m_numSavedFrames; i++)
  {
    for (int j=0; j<m_numLayers; j++)
    {
      for (int k=0; k<m_numElectrolyteSpecies; k++)
      {
        cout << m_numIonsInLayerTime[i][j][k] << " ";
      }
      cout << endl;
    }
  }
}

void AtomCounter::printDensity()
{
  for (int i=0; i<m_numBins; i++)
  {
    cout << i*getBinSize() << " " << m_densityProfile[i] << endl;
  }
  cout << endl;
}

const int AtomCounter::getNumBins() const
{
  return m_numBins;
}

const double AtomCounter::getBinSize() const
{
  return m_binSize;
}

double* AtomCounter::getACAtomsAddress(int i)
{
  return &(m_numAtomsProfile[i][0]);
}

double* AtomCounter::getACDensityAddress(int i)
{
  return &(m_densityProfile[i]);
}

double* AtomCounter::getACElectrodeChargeAddress(int i)
{
  return &(m_electrodeChargeProfile[i]);
}

double* AtomCounter::getACIonsAddress(int i)
{
  return &(m_numIonsProfile[i][0]);
}

double* AtomCounter::getACIonsLayersAddress(int i)
{
  return &(m_avgIonsInLayer[i][0]);
}

double* AtomCounter::getACIonsLayersTimeAddress(int i, int j)
{
  return &(m_numIonsInLayerTime[i][j][0]);
}

double* AtomCounter::getACElecChargeSlicesTimeAddress(int i, int j)
{
  return &(m_elecSliceCharge[i][j][0]);
}

double* AtomCounter::getACElecNumIonsSlicesTimeAddress(int i, int j)
{
  return &(m_elecSliceNumIons[i][j][0]);
}

double* AtomCounter::getChargingParamAddress(int i)
{
  return &(m_chargingParam[i][0]);
}

array<double, 2> AtomCounter::computeChargingParam(vector<array<int, NUM_ION_TYPES> >& a_ionsInLayer)
{
  array<double, 2> retArray;
  array<array<int, 3>, 2> indices;
  indices[0][0]=0;
  indices[0][1]=0;
  indices[0][2]=1;
  indices[1][0]=2;
  indices[1][1]=1;
  indices[1][2]=0;
  for (int i=0; i<2; i++)
  {
    int layer=indices[i][0];
    int counter=indices[i][1];
    int co=indices[i][2];
    // N(V) total number of in-pore ions at charging voltage V
    int NV = a_ionsInLayer[layer][co] + a_ionsInLayer[layer][counter];
    // N(V0) total number of in-pore ions at initial voltage V0
    int NV0 = m_avgZPIonsInLayer[layer][co] + m_avgZPIonsInLayer[layer][counter];
    // Nco,Ncounter(V) number of in-pore co- and counter-ions at charging voltage V
    int NcounterV = a_ionsInLayer[layer][counter];
    int NcoV = a_ionsInLayer[layer][co];
    // Nco,Ncounter(V0) number of in-pore co- and counter-ions at initial voltage V0
    int NcounterV0 = m_avgZPIonsInLayer[layer][counter];
    int NcoV0 = m_avgZPIonsInLayer[layer][co];
    double retVal = NcounterV-NcoV - NcounterV0+NcoV0;
    retVal = 1./retVal;
    retVal = retVal * (NV - NV0);
    retArray[i] = retVal;
  }
  return retArray;
}

const System& AtomCounter::getSystem() const
{
  return m_system;
}

const int AtomCounter::getSaveFrameInterval() const
{
  return m_saveFrameEvery;
}

const int AtomCounter::getNumSavedFrames() const
{
  return m_numSavedFrames;
}

const int AtomCounter::getNumCVFrames() const
{
  return m_system.getNumFrames();
}

const double AtomCounter::getBinSizeAtomCharge() const
{
  return m_binSizeAtomCharge;
}
const int AtomCounter::getNumBinsAtomCharge() const
{
  return m_numBinsAtomCharge;
}
const double AtomCounter::getMaxAtomCharge() const
{
  return m_maxAtomCharge;
}
double* AtomCounter::getElecAtomChargeHistAddress(int i)
{
  return &(m_elecAtomChargeHist[i][0]);
}
const int AtomCounter::getNumElectrolyteSpecies() const
{
  return m_numElectrolyteSpecies;
}

// const vector<array<double, DIM >  > AtomCounter::getCOMs() const
// {
//   return m_COMs;
// }


const char* ACWriteAtomCounts(AtomCounter* a_ac, const char* a_filename)
{
  double binSize = a_ac->getBinSize();
  int numBins = a_ac->getNumBins();
  int varDim = a_ac->getNumAtomTypes();
  const char * const headernames[] = { "z[A]",  "0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",  "8",  "9",  "10",  "11",  "12",  "13", "14" };
  double** data;
  data = new double* [numBins];
  for (int i=0; i<numBins; i++)
  {
    data[i] = a_ac->getACAtomsAddress(i);
  }
  write_binned_data(a_filename, numBins, binSize, varDim, headernames, data);
  delete data;
  return a_filename;
}

const char* ACWriteDensity(AtomCounter* a_ac, const char* a_filename)
{
  double binSize = a_ac->getBinSize();
  int numBins = a_ac->getNumBins();
  int varDim = 1;
  const char * const headernames[] = { "z[A]", "dens[g/ml]" };
  double** data;
  data = new double* [numBins];
  for (int i=0; i<numBins; i++)
  {
    data[i] = a_ac->getACDensityAddress(i);
  }
  write_binned_data(a_filename, numBins, binSize, varDim, headernames, data);
  delete data;
  return a_filename;
}

const char* ACWriteElectrodeCharge(AtomCounter* a_ac, const char* a_filename)
{
  double binSize = a_ac->getBinSize();
  int numBins = a_ac->getNumBins();
  int varDim = 1;
  const char * const headernames[] = { "z[A]", "charge[e]" };
  double** data;
  data = new double* [numBins];
  for (int i=0; i<numBins; i++)
  {
    data[i] = a_ac->getACElectrodeChargeAddress(i);
  }
  write_binned_data(a_filename, numBins, binSize, varDim, headernames, data);
  delete data;
  return a_filename;
}

const char* ACWriteIons(AtomCounter* a_ac, const char* a_filename)
{
  double binSize = a_ac->getBinSize();
  int numBins = a_ac->getNumBins();
  int varDim = a_ac->getNumIonTypes();
  const char * const headernames[] = { "z[A]", "cation", "anion", "solvent" };
  double** data;
  data = new double* [numBins];
  for (int i=0; i<numBins; i++)
  {
    data[i] = a_ac->getACIonsAddress(i);
  }
  write_binned_data(a_filename, numBins, binSize, varDim, headernames, data);
  delete data;
  return a_filename;
}

const char* ACWriteIonsInLayers(AtomCounter* a_ac, const char* a_filename)
{
  int numLayers = a_ac->getNumLayers();
  double* layers;
  layers = new double[numLayers];
  a_ac->getSystem().getLayerUpperBounds(numLayers,layers);
  int varDim = a_ac->getNumIonTypes();
  const char * const headernames[] = { "z[A]", "cation", "anion", "solvent" };
  double** data;
  data = new double* [numLayers];
  for (int i=0; i<numLayers; i++)
  {
    data[i] = a_ac->getACIonsLayersAddress(i);
  }
  write_layered_data(a_filename, numLayers, layers, varDim, headernames, data);
  delete data, layers;
  return a_filename;
}

const char* ACWriteIonsInLayersTime(AtomCounter* a_ac, const char* a_filename)
{
  int numFrames = a_ac->getNumSavedFrames();
  float saveFrameEvery = a_ac->getSaveFrameInterval()*a_ac->getSystem().getFrameTime();
  int numLayers = a_ac->getNumLayers();
  int varDim = a_ac->getNumIonTypes();
  const char * const headernames[] = { "t", "data" };
  double*** data;
  data = new double** [numFrames];
  for (int i=0; i<numFrames; i++)
  {
    data[i] = new double* [numLayers];
    for (int j=0; j<numLayers; j++)
    {
      data[i][j] = a_ac->getACIonsLayersTimeAddress(i,j);
    }
  }
  write_layered_time_data_by_var(a_filename, numFrames, saveFrameEvery, numLayers, varDim, headernames, data);
  delete data;
  return a_filename;
}

const char* ACWriteElecChargeSlicesTime(AtomCounter* a_ac, const char* a_filename)
{
  int numFrames = a_ac->getNumSavedFrames();
  float saveFrameEvery = a_ac->getSaveFrameInterval()*a_ac->getSystem().getFrameTime();
  int numLayers = NUM_ELEC_SLICES;
  int varDim = 2; // number of electrodes is always 2
  const char * const headernames[] = { "t", "data" };
  double*** data;
  data = new double** [numFrames];
  for (int i=0; i<numFrames; i++)
  {
    data[i] = new double* [numLayers];
    for (int j=0; j<numLayers; j++)
    {
      data[i][j] = a_ac->getACElecChargeSlicesTimeAddress(i,j);
    }
  }
  write_layered_time_data_by_var(a_filename, numFrames, saveFrameEvery, numLayers, varDim, headernames, data);
  delete data;
  return a_filename;
}

const char* ACWriteElecNumIonsSlicesTime(AtomCounter* a_ac, const char* a_filename)
{
  int numFrames = a_ac->getNumSavedFrames();
  float saveFrameEvery = a_ac->getSaveFrameInterval()*a_ac->getSystem().getFrameTime();
  int numLayers = NUM_ELEC_SLICES*a_ac->getNumElectrolyteSpecies();
  int varDim = 2; // number of electrodes is always 2
  const char * const headernames[] = { "t", "data" };
  double*** data;
  data = new double** [numFrames];
  for (int i=0; i<numFrames; i++)
  {
    data[i] = new double* [numLayers];
    for (int j=0; j<numLayers; j++)
    {
      data[i][j] = a_ac->getACElecNumIonsSlicesTimeAddress(i,j);
    }
  }
  write_layered_time_data_by_var(a_filename, numFrames, saveFrameEvery, numLayers, varDim, headernames, data);
  delete data;
  return a_filename;
}

const char* ACWriteCollectiveVars(AtomCounter* a_ac, const char* a_filename)
{
  int numFrames = a_ac->getNumCVFrames();
  float saveFrameEvery = a_ac->getSaveFrameInterval();
  int varDim = 2;
  const char * const headernames[] = {"t", "data"};
  double** data;
  data = new double* [numFrames];
  for (int i=0; i<numFrames; i++)
  {
    data[i] = a_ac->getChargingParamAddress(i);
  }
  write_time_data(a_filename, numFrames, saveFrameEvery, varDim, headernames, data);
  delete data;
}

const char* ACWriteElecAtomChargeHist(AtomCounter* a_ac, const char* a_filename)
{
  double binSize = a_ac->getBinSizeAtomCharge();
  int numBins = a_ac->getNumBinsAtomCharge();
  int varDim = a_ac->getSystem().getNumEleGrps();
  double binOffset = -1.0*a_ac->getMaxAtomCharge();
  const char * const headernames[] = { "q[e]",  "0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",  "8",  "9",  "10",  "11",  "12",  "13", "14", "15", "16", "17", "18", "19", "20" };
  double** data;
  data = new double* [numBins];
  for (int i=0; i<numBins; i++)
  {
    data[i] = a_ac->getElecAtomChargeHistAddress(i);
  }
  write_binned_data(a_filename, numBins, binSize, binOffset, varDim, headernames, data);
  delete data;
  return a_filename;
}
