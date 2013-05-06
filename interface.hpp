#ifndef INTERFACE_HPP
#define INTERFACE_HPP

#include "filedriverbase.hpp"

class D64;
class T64;
class M2I;

class Interface
{
public:
  Interface();
private:
  D64 m_d64;
  T64 m_t64;
  M2I m_m2i;
  FileDriverBase* m_currFileDriver;
};

#endif // INTERFACE_HPP
