#include <fstream>
#include <istream>
#include <iostream>
#include <cassert>
#include "math.h"
#include "atom.h"
#include "system.h"
#include "frame.h"

using namespace std;

Frame::Frame() {
};
Frame::Frame(System& a_system)
{
  m_system = a_system;
  m_stepNum = -1;
  m_numAtoms = a_system.getNumAtoms();
  m_traj.open(a_system.getTrajFile());
  int trajNumAtoms;
  m_traj >> trajNumAtoms;
  assert(m_numAtoms==trajNumAtoms);
  m_atoms.resize(m_numAtoms);
  m_traj.seekg(0, m_traj.beg);
};
void Frame::readStep()
{
  // read step from traj file
  m_stepNum++;
  char tmp[256];
  m_traj >> tmp >> tmp >> tmp >> tmp;
  string atomName;
  double x, y, z;
  for (unsigned int i=0; i<m_numAtoms; i++)
    {
      m_traj >> atomName >> x >> y >> z;
      m_atoms[i].setPosition(x,y,z);
    }
}
unsigned int Frame::getStepNum()
{
  return m_stepNum;
}
void Frame::clearFrame()
{
  for (int i=0; i<m_numAtoms; i++)
    {
      m_atoms[i].setPosition(-1, -1, -1);
    }
}
Atom& Frame::getAtom(int a_atomIndex)
{
  return m_atoms[a_atomIndex];
}

double Frame::computeDistance(int a_i, int a_j)
{
  array<double, DIM > boxDims = m_system.getBoxDims();
  array<double, DIM > posI = m_atoms[a_i].getPosition();
  array<double, DIM > posJ = m_atoms[a_j].getPosition();
  double retVal = 0.0;
  for (int i=0; i<DIM; i++)
    {
      double dist = abs(posI[i] - posJ[i]);
      if(m_system.isPeriodic(i))
	{
	  dist -= boxDims[i]*floor(dist/(0.5*boxDims[i]));
	}
      retVal += dist*dist;
    }
  retVal = sqrt(retVal);
  return retVal;
}

