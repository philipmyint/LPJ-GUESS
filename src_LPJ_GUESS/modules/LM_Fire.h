///////////////////////////////////////////////////////////////////////////////////////
// MODULE HEADER FILE
//
// Module:                LPJ-GUESS module calculates fire ignition, spread, and effects
//							based on the lpjlmfirecode
// Header file name:      LM_FireOccurrence.h
// Source code file name: LM_FireOccurrence.cpp
// Written by:            Kristen Emmett
// Adapted from:		  lpjlmfirecode spitfiremod.f90 code
//							Pfeiffer M, Spessa A, Kaplan JO. 2013. Geoscientific Model Development 6:643–85.
//							Thonicke K, Spessa A, Prentice IC, Harrison SP, Dong L, Carmona-Moreno C. 2010. Biogeosciences 7:1991–2011.
// Version dated:         2018-8-24
// Updated:

/// Modified by:            Weichao Guo(some codes from Kristen Emmett)
/// Version dated:         2021-08-25
/// Updated:               2021-08-28  


// WHAT SHOULD THIS FILE CONTAIN?
// Module header files need normally contain only declarations of functions defined in
// the module that are to be accessible to the calling framework or to other modules.

#ifndef LM_FIRE_H
#define LM_FIRE_H

#include "guess.h"
#include "cruinput.h"

// void lmfire (Patch& patch, Gridlist& gridlist, Stand& stand, Pftlist& pftlist);
void lmfire (Patch& patch, Stand& stand, Gridcell& gridcell, Pftlist& pftlist); // modified by weichao

#endif //LM_FIRE_H
