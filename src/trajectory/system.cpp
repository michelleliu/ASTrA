#include <math.h>
#include <iostream>
#include <sstream>
#include <cstring>
#include <array>
#include <fstream>
#include <string>
#include "system.h"
#include <assert.h>

using namespace std;

System::System()
{
};

// Reads input to get information on
// system size, number of atoms, type information, etc.
// as well as information on the RDF pairs to compute
System::System(const string& a_inputFile)
{
    readInput(a_inputFile);
    setInput();

 

};

void System::printPairCorrelations() const
{
  for (unsigned int i=0; i<m_numPairs; i++)
    {
      cout << "pair " << i << " : ";
      cout << m_rdfPairs[i].first << " " << m_rdfPairs[i].second << endl;
      
    }
}

void System::printTypeAtomIndices() const
{
  for (unsigned int i=0; i<m_numAtomTypes; i++)
    {
      cout << "type " << i << " : " << endl;
      for (unsigned int j=0; j<m_typeAtomIndices[i].size() ; j++)
	{
	  cout << m_typeAtomIndices[i][j] << " ";
	}
      cout << endl;
    }
}

template <typename T>
T convert_to (const std::string &a_str)
{
    istringstream ss(a_str);
    T retVal;
    ss >> retVal;
    return retVal;
}

template <typename T>
void System::getInput(T* a_value, int a_col)
{
  *a_value = convert_to<T>(m_inputs[m_inputRow][a_col]);
}

template <typename T1, typename T2>
void System::getInputs2(T1* a_value1, T2* a_value2)
{
  *a_value1 = convert_to<T1>(m_inputs[m_inputRow][0]);
  *a_value2 = convert_to<T2>(m_inputs[m_inputRow][1]);
}


template <typename T1, typename T2, typename T3>
void System::getInputs3(T1* a_value1, T2* a_value2, T3* a_value3)
{
  *a_value1 = convert_to<T1>(m_inputs[m_inputRow][0]);
  *a_value2 = convert_to<T2>(m_inputs[m_inputRow][1]);
  *a_value3 = convert_to<T3>(m_inputs[m_inputRow][3]);
}

template <typename T1, typename T2>
void System::getInputs2(T1* a_value1, T2* a_value2, int a_offset)
{
  *a_value1 = convert_to<T1>(m_inputs[m_inputRow][0+a_offset]);
  *a_value2 = convert_to<T2>(m_inputs[m_inputRow][1+a_offset]);
}

template <typename T1, typename T2, typename T3>
void System::getInputs3(T1* a_value1, T2* a_value2, T3* a_value3, int a_offset)
{
  *a_value1 = convert_to<T1>(m_inputs[m_inputRow][0+a_offset]);
  *a_value2 = convert_to<T2>(m_inputs[m_inputRow][1+a_offset]);
  *a_value3 = convert_to<T3>(m_inputs[m_inputRow][3+a_offset]);
}


void System::nextRow()
{
  m_inputRow++;
}

void System::setInput()
{
  m_inputRow = 0;
  getInput(&m_trajFile, 0);

  nextRow();
  getInput(&m_numFrames,0);
  
  nextRow();
  for (int i=0; i<DIM; i++)
    {
      getInput(&m_boxDims[i],i);
    }
  for (int i=0; i<DIM; i++)
    {
      getInput(&m_boxPeriodic[i],i+2);
    }

  nextRow();
  getInputs2(&m_lowerElecTop,&m_upperElecBot);

  nextRow();
  getInput(&m_numMolecTypes,0);

  m_numAtomTypes = 0;
  m_numMolecules = 0;
  m_numAtoms = 0;
  for (unsigned int i=0; i<m_numMolecTypes; i++)
    {
      nextRow();
      getInputs2(&m_numMolecs[i],&m_numMembersMolec[i]);
      m_numMolecules += m_numMolecs[i];
      m_numAtomTypes += m_numMembersMolec[i];
      m_numAtoms += m_numMolecs[i]*m_numMembersMolec[i];

      for (unsigned int j=0; j<m_numMembersMolec[i]; j++)
	{
	  nextRow();
	  getInputs2(&m_masses[i][j], &m_charges[i][j]);
	}
    }

  nextRow();
  getInputs2(&m_cationID, &m_anionID);
  getInputs2(&m_lowerElecID,&m_upperElecID,2);

  nextRow();
  getInput(&m_anodeIsLower,0);
  if (m_anodeIsLower)
    {
      m_anodeID = m_lowerElecID;
      m_cathodeID = m_upperElecID;
    }
  else
    {
      m_anodeID = m_upperElecID;
      m_cathodeID = m_lowerElecID;
    }

  nextRow();
  getInput(&m_solventID,0);
  if (m_solventID == 0)
    {
      m_boolWithSolvent = 0;
      m_numElectrolyteSpecies = 2;
      m_numElectrolyteMolecs = getNumMolecsOfType(m_cationID) + getNumMolecsOfType(m_anionID);
    }
  else
    {
      m_boolWithSolvent = 1;
      m_numElectrolyteSpecies = 3;
      m_numElectrolyteMolecs = getNumMolecsOfType(m_cationID) + getNumMolecsOfType(m_anionID) + getNumMolecsOfType(m_solventID);
    }

  nextRow();
  getInput(&m_capID,0);
  if (m_capID == 0) m_boolWithCap = 0;
  else m_boolWithCap = 0;

  cout << "number of atom types: " << m_numAtomTypes << endl;

  nextRow();
  getInput(&m_stepInterval,0);

  nextRow();
  getInput(&m_stepTime,0);

  nextRow();
  getInput(&m_numPairs,0);
  m_rdfPairs.resize(m_numPairs);

  cout << "Pair correlations:" << endl;

  for (unsigned int i=0; i<m_numPairs; i++)
    {
      unsigned int molecA, atomA, molecB, atomB;

      nextRow();
      getInputs2(&molecA,&atomA);
      getInputs2(&molecB,&atomB,2);
      unsigned int atomTypeA = getAtomType(molecA-1, atomA-1);
      unsigned int atomTypeB = getAtomType(molecB-1, atomB-1);
      cout << i << " " << atomTypeA << " " << atomTypeB << endl;
      m_rdfPairs[i] = make_pair(atomTypeA, atomTypeB);
    }
    
   m_frameTime = m_stepInterval * m_stepTime;
    m_typeAtomIndices.resize(m_numAtomTypes);
    m_molecMembersOfType.resize(m_numAtomTypes);
    unsigned int atomTypeCounter=0;
    for (unsigned int i=0; i<m_numMolecTypes; i++)
      {
	for (unsigned int j=0; j<m_numMolecs[i]; j++)
	  {
	    for (unsigned int k=0; k<m_numMembersMolec[i]; k++)
	      {
		m_typeAtomIndices[getAtomType(i,k)].push_back(atomTypeCounter);
		atomTypeCounter++;
	      }
	  }
      }
    for (unsigned int i=0; i<m_numMolecTypes; i++)
      {
	for (unsigned int j=0; j<m_numMembersMolec[i]; j++)
	  {
	    m_molecMembersOfType[getAtomType(i,j)] = make_pair(i+1,j+1);
	  }
      }
}


void System::readInput(const string& a_inputFile)
{
  // read string
  ifstream system(a_inputFile.c_str());
  string delimiter=" ";
  char inputline[256];
  while ( ! system.eof() )
    {
      system.getline(inputline,256);
      //inputs.push_back(readNextLine(inputline, delimiter));
      vector<string > newLine = readNextLine(inputline, delimiter);
      if( newLine.size() > 0 )
	{
	  m_inputs.push_back(newLine);
	}
   }
#ifdef DEBUG
  for (int i=0; i<m_inputs.size(); i++)
    {
      for(int j=0; j<m_inputs[i].size(); j++)
  	{
  	  cout << m_inputs[i][j] << " ";
  	}
      cout << endl;
    }
#endif
 
}

vector<string > System::readNextLine(char* a_inputline, string& a_delimiter)
{
  char ignoreChar[] = "#";
  if( strncmp(a_inputline,ignoreChar,1) != 0 )
    {
      return lineToString(a_inputline, a_delimiter);
    }
  else
    {
      //cout << "comment found!" << endl;
      vector<string > newVector;
      //return null;
      return newVector;
    }
}

vector<string > System::lineToString(char* a_inputline, string& a_delimiter)
{
  vector<string > retVector;
  string s(a_inputline);
  size_t pos = 0;
  std::string token;
  while ((pos = s.find(a_delimiter)) != string::npos) {
    token = s.substr(0, pos);
    retVector.push_back(token);
    s.erase(0, pos + a_delimiter.length());
  }
  retVector.push_back(s);
  return retVector;
}
void System::readInputOld(const string& a_inputFile)
{
    ifstream system(a_inputFile.c_str());
    cout << "Reading input file ";
    cout << a_inputFile << endl;

    system >> m_trajFile;
    system >> m_numFrames;
    
    for (int i=0; i<DIM; i++) {
        system >> m_boxDims[i];
    }
    for (int i=0; i<DIM; i++) {
        system >> m_boxPeriodic[i];
    }
    cout << m_trajFile << endl;
    cout << "Box dims: ";
    for (int i=0; i<DIM; i++) {
        cout << m_boxDims[i] << " ";
    }
    cout << endl;
    system >> m_lowerElecTop >> m_upperElecBot;
    cout << "Electrode limits: " << m_lowerElecTop << " " << m_upperElecBot << endl;
    system >> m_numMolecTypes;
    m_numAtomTypes = 0;
    for (unsigned int i=0; i<m_numMolecTypes; i++)
    {
        system >> m_numMolecs[i] >> m_numMembersMolec[i];
	m_numMolecules += m_numMolecs[i];
	m_numAtomTypes += m_numMembersMolec[i];
	m_numAtoms += m_numMolecs[i]*m_numMembersMolec[i];
	for (unsigned int j=0; j<m_numMembersMolec[i]; j++)
	{
	  system >> m_masses[i][j] >> m_charges[i][j];
	}
    }
    system >> m_cationID >> m_anionID >> m_lowerElecID >> m_upperElecID;
    system >> m_anodeIsLower ;
    if (m_anodeIsLower)
      {
	m_anodeID = m_lowerElecID;
	m_cathodeID = m_upperElecID;
      }
    else
      {
	m_anodeID = m_upperElecID;
	m_cathodeID = m_lowerElecID;
      }
    system >> m_solventID ;
    system >> m_capID ;
    if (m_solventID == 0) {
      m_boolWithSolvent = 0;
      m_numElectrolyteSpecies = 2;
      m_numElectrolyteMolecs = getNumMolecsOfType(m_cationID) + getNumMolecsOfType(m_anionID);
    }
    else {
      m_boolWithSolvent = 1;
      m_numElectrolyteSpecies = 3;
      m_numElectrolyteMolecs = getNumMolecsOfType(m_cationID) + getNumMolecsOfType(m_anionID) + getNumMolecsOfType(m_solventID);
    }

    if (m_capID == 0) {
      m_boolWithCap = 0; }
    else {
      m_boolWithCap = 0; }
    cout << "number of atom types: " << m_numAtomTypes << endl;
    system >> m_stepInterval >> m_stepTime;
    system >> m_numPairs;
    m_rdfPairs.resize(m_numPairs);
    cout << "Pair correlations:" << endl;
    for (unsigned int i=0; i<m_numPairs; i++) {
      unsigned int molecA, atomA, molecB, atomB;
      system >> molecA >> atomA >> molecB >> atomB;
      unsigned int atomTypeA = getAtomType(molecA-1, atomA-1);
      unsigned int atomTypeB = getAtomType(molecB-1, atomB-1);
      cout << i << " " << atomTypeA << " " << atomTypeB << endl;
      m_rdfPairs[i] = make_pair(atomTypeA, atomTypeB);
    }
    
}


const int System::getNumAtoms() const
{
  return m_numAtoms;
}

const int System::getNumAtomTypes() const
{
  return m_numAtomTypes;
}


const int System::getNumMolecTypes() const
{
  return m_numMolecTypes;
}

const int System::getNumMolecsOfType(unsigned int a_molecType) const
{
  return m_numMolecs[a_molecType];
}

const int System::getNumMembersMolec(unsigned int a_molecType) const
{
  return m_numMembersMolec[a_molecType];
}

const unsigned int System::getNumMolecules() const
{
  return m_numMolecules;
}

const float System::getFrameTime() const
{
    return m_frameTime;
}

const string& System::getTrajFile() const
{
    return m_trajFile;
}

const unsigned int System::getNumOfType(unsigned int a_type) const
{
  return m_typeAtomIndices[a_type].size();
}

const unsigned int System::getIndexOfType(unsigned int a_type, unsigned int a_idx) const
{
  return m_typeAtomIndices[a_type][a_idx];
}

const unsigned int System::getMolecIndex(unsigned int a_type, unsigned int a_member) const
{
  unsigned int retVal = 0;
  for (unsigned int i=0; i<a_type; i++)
    {
      for (unsigned int j=0; j<a_member; j++)
	{
	  retVal++;
	}
    }
  return retVal;
}

const unsigned int System::getAtomType(unsigned int a_molecType, unsigned int a_molecAtom) const
{
  unsigned int retVal = 0;
  for (unsigned int i=0; i<a_molecType; i++)
    {
      retVal += m_numMembersMolec[i];
    }
  retVal += a_molecAtom;
  return retVal;
}

const unsigned int System::getNumPairs() const
{
  return m_numPairs;
}

const pair<unsigned int, unsigned int > System::getPairCorrelation(unsigned int a_pair) const
{
  return m_rdfPairs[a_pair];
}

const unsigned int System::getNumFrames() const
{
  return m_numFrames;
}

const array<double, DIM >& System::getBoxDims() const
{
  return m_boxDims;
}

const double System::getBoxDim(int a_dim) const
{
  return m_boxDims[a_dim];
}

const unsigned int System::isPeriodic(int i) const
{
  return m_boxPeriodic[i];
}

const unsigned int System::getNumLayers() const
{
  return 3;
}
const unsigned int System::getNumElectrolyteSpecies() const
{
  return m_numElectrolyteSpecies;
}

const unsigned int System::getNumElectrolyteMolecs() const
{
  return m_numElectrolyteMolecs;
}


const unsigned int System::getLayer(array<double, DIM>& a_position) const
{
  unsigned int retVal;
  double z=a_position[DIM-1];
  if (z < m_lowerElecTop )
    {
      retVal = 0;
    }
  else if (z < m_upperElecBot )
    {
      retVal = 1;
    }
  else
    {
      retVal = 2;
    }
  return retVal;
}

const array<double, MAX_MEMBERS_PER_MOLEC > System::getMassesOfType(int a_type) const
{
  return m_masses[a_type];
}

const array<double, MAX_MEMBERS_PER_MOLEC > System::getChargesOfType(int a_type) const
{
  return m_charges[a_type];
}

unsigned int System::isElectrolyte(int a_molecType, int* a_electrolyteID) const
{
  if (a_molecType == m_cationID-1)
    {
      *a_electrolyteID = 0;
      return 1;
    }
  else if (a_molecType == m_anionID-1)
    {
      *a_electrolyteID = 1;
      return 1;
    }
  else if (a_molecType == m_solventID-1)
    {
      *a_electrolyteID = 2;
      return 1;
    }
  else
    {
      *a_electrolyteID = -1;
      return 0;
    }
}

void System::getLayerUpperBounds(int a_numLayers, double* a_layers) const
{
  assert(a_numLayers == 3);
  a_layers[0] = m_lowerElecTop;
  a_layers[1] = m_upperElecBot;
  a_layers[2] = m_boxDims[2];
}
