///////////////////////////////////////////////////////////////////////////////////////
/// \file driver.h
/// \brief Environmental driver calculation/transformation
///
/// \author Ben Smith
/// $Date: 2014-06-23 15:50:25 +0200 (Mon, 23 Jun 2014) $
///
/// Modified by:            Weichao Guo(some codes from Kristen Emmett)
/// Version dated:         2021-08-25
/// Updated:               2021-08-28  
///////////////////////////////////////////////////////////////////////////////////////

// WHAT SHOULD THIS FILE CONTAIN?
// Module header files need normally contain only declarations of functions defined in
// the module that are to be accessible to the calling framework or to other modules.

#ifndef LPJ_GUESS_DRIVER_H
#define LPJ_GUESS_DRIVER_H

#include "guess.h"
#include <limits>

double randfrac(long& seed);
void soilparameters(Soiltype& soiltype,int soilcode,double SOILDEPTH_UPPER,double SOILDEPTH_LOWER);
void interp_monthly_means_conserve(const double* mvals, double* dvals,
                                   double minimum = -std::numeric_limits<double>::max(),
                                   double maximum = std::numeric_limits<double>::max());
void interp_monthly_totals_conserve(const double* mvals, double* dvals,
                                   double minimum = -std::numeric_limits<double>::max(),
                                   double maximum = std::numeric_limits<double>::max());
void interp_climate(double mtemp[12], double mprec[12], double msun[12],
		double mwind[12], double mtmin[12], double mtmax[12],double mlght[12],
		double dtemp[365], double dprec[365], double dsun[365],
		double dwind[365], double dtmin[365], double dtmax[365],double dlght[12],
		double dNI[365]);								   
void distribute_ndep(const double* mndry, const double* mnwet,
                     const double* dprec, double* dndep);
void prdaily(double* mval_prec, double* dval_prec, double* mval_wet, long& seed, bool truncate = true);
void dailyaccounting_gridcell(Gridcell& gridcell);
void dailyaccounting_stand(Stand& stand);
void dailyaccounting_patch(Patch& patch);
void respiration_temperature_response(double temp,double& gtemp);
void daylengthinsoleet(Climate& climate);
void soiltemp(Climate& climate,Soil& soil);

// KMR adds:
void sagebrush_recruitment(Climate& climate, Soil& soil);
// KE adds:
void esat(Climate& climate);
#endif // LPJ_GUESS_DRIVER_H
