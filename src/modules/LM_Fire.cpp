///////////////////////////////////////////////////////////////////////////////////////
// MODULE SOURCE CODE FILE
//
// Module:                 LPJ-GUESS module calculates fire ignition, spread, and effects
//							based on the lpjlmfirecode
// Header file name:      LM_Fire.h
// Source code file name: LM_Fire.cpp
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
// Module source code files should contain, in this order:
//   (1) a "#include" directive naming the framework header file. The framework header
//       file should define all classes used as arguments to functions in the present
//       module. It may also include declarations of global functions, constants and
//       types, accessible throughout the model code;
//   (2) other #includes, including header files for other modules accessed by the
//       present one;
//   (3) type definitions, constants and file scope global variables for use within
//       the present module only;
//   (4) declarations of functions defined in this file, if needed;
//   (5) definitions of all functions. Functions that are to be accessible to other
//       modules or to the calling framework should be declared in the module header
//       file.
//
// PORTING MODULES BETWEEN FRAMEWORKS:
// Modules should be structured so as to be fully portable between models (frameworks).
// When porting between frameworks, the only change required should normally be in the
// "#include" directive referring to the framework header file.

#include "LM_Fire.h"
#include "config.h"
#include "driver.h" //linked for randfrac()
#include "stdio.h"	//to print typedef enum
#include <iostream>
#include "cru_ts30.h"

// extern double leaf_littter;
// extern double root_littter;
// extern double wood_littter;

///////////////////////////////////////////////////////////////////////////////////////
// FILE SCOPE GLOBAL CONSTANTS
const double c2om = 1. / 0.45; //! conversion factor between carbon and total mass of vegetation
const double ST = 0.055;	   //! fraction of total vegetation mass that is mineral (non-flammable), Thonicke 2010 BGS
const double orgf = 1 - ST;	   //! organic fraction of biomass
const double h = 18.;		   //! heat content of fuel (kJ g-1)
// const double PI = 3.14159265;
const double min2sec = 1. / 60.;

//! PFT-specific parameters for fire damage (only for trees) CHANGE
// For simplicity all conifers set to BNE values //@KE changed for GYE PFTS, must update based on number/type of pfts -- could add to ins file!
// const double ieffpft[] = { 0.44, 0.44, 0.44, 0.44, 0.44, 0.5, 0.44, 0.44, 0.5, 0.5, 0.5 }; 	//ieffpft !pft-dependent ignition efficiency parameters
// const double ieffpft[] = { 0.10, 0.10, 0.10, 0.10, 0.10, 0.10, 0.10, 0.10, 0.5, 0.5, 0.5 }; 	//ieffpft !pft-dependent ignition efficiency parameters TESTING AUG
const double ieffpft[] = {0.10, 0.10, 0.10, 0.10, 0.10, 0.10, 0.5, 0.10, 0.10, 0.5}; // ieffpft !pft-dependent ignition efficiency parameters TESTING AUG

// double rhobPFT[] = { 15., 15., 15., 15., 15., 15., 15., 15., 2., 2., 2. }; 					//!fuel bulk density parameters (kg m-3) !these values are averages from different USFS fuel models
double rhobPFT[] = {15., 15., 15., 15., 15., 15., 2., 15., 15., 2.}; //! fuel bulk density parameters (kg m-3) !these values are averages from different USFS fuel models

// double rhobPFT[] = { 11.5, 11.5, 11.5, 11.5, 11.5, 11.5, 11.5, 11.5, 2., 2., 2. }; 					//!fuel bulk density parameters (kg m-3) !these values are averages from different USFS fuel models
// double F[] = { 0.094, 0.094, 0.094, 0.094, 0.094, 0.094, 0.094, 0.094, -99, -99, -99 };  	//!scorch height parameters
//  double F[] = { 0.148, 0.148, 0.148, 0.148, 0.148, 0.148, 0.148, 0.148, -99, -99, -99 };  	//!scorch height parameters //Van Wagner TESTING JULY
double F[] = {0.148, 0.148, 0.148, 0.148, 0.148, 0.148, -99, 0.148, 0.148, -99}; //! scorch height parameters //Van Wagner TESTING JULY
// double par1[] = { 0.0292, 0.0292, 0.0292, 0.0292, 0.0292, 0.0347, 0.0292, 0.0292, -99, -99, -99 };	//!bark thickness 1 slope
// double par2[] = { 0.1086, 0.1086, 0.1086, 0.1086, 0.1086, 0.1086, 0.1086, 0.1086, -99, -99, -99 };	//!bark thickness 2 intercept
//  order for 2.7.19.ins PICO, CW_con, PSME, hi_pines, PIPO, POTR, PJ, ARTR, BRTE, C3, C4
// double BTs[] = { 0.01, 0.021, 0.048, 0.0098, 0.01, 0.027, 0.031, 0.031, -99, -99, -99 };	//bark thickness slope (par1)
// double BTi[] = { 0.22, 0.21, 0.41, 0.44, 0.22, 0.29, 0.067, 0.067, -99, -99, -99 };		//bark thickness intercept (par2)

// double m_ex[] = { 0.35, 0.35, 0.35, 0.35, 0.35, 0.3, 0.35, 0.35, 0.2, 0.2, 0.2 };  			//!moisture of extinction //CHANGE! Not called anywhere!

//@KE added for calculating crown length based on height for each PFT
// order for 2.7.19.ins PICO, CW_con, PSME, hi_pines, PIPO, POTR, PJ, ARTR, BRTE, C3, C4
// double CLs[] = { 0.31, 0.83, 0.69,  0.67, 0.31, 0.24, 0.78, 1.0, -99., -99., -99. };  	//crown length slope
// double CLi[] = { 1.8, -0.3, 0.034, 0.67, 1.8, 1.9, 0.66, 0.0, -99., -99., -99. };  		//crown length intercept

double emCO2[] = {1568., 1568., 1568., 1568., 1568., 1568., 1568., 1568., 1568., 1664.}; //! emission factor for CO2
double emCO[] = {106., 106., 106., 106., 106., 106., 106., 106., 106., 63.};			 //! emission factor for CO
double emCH4[] = {4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 2.2};					 //! emission factor for CH4
double emVOC[] = {5.7, 5.7, 5.7, 5.7, 5.7, 5.7, 5.7, 5.7, 5.7, 3.4};					 //! emission factor for VOC
double emTPM[] = {17.6, 17.6, 17.6, 17.6, 17.6, 17.6, 17.6, 17.6, 17.6, 8.5};			 //! emission factor for TPM
double emNOx[] = {3.24, 3.24, 3.24, 3.24, 3.24, 3.24, 3.24, 3.24, 3.24, 2.54};			 //! emission factor for NOx from Allan's F77 code

//!-----------------
//! parameters for characterization of fuel (bulk density and surface area to volume ratio)
const double rho_soilC = 75.;
const double cm2ft = 30.48; //! cm in one foot, length conversion factor
//! Surface-area-to-volume ratios of fuel classes
const double surf2vol[] = {2012., 109., 30.};											  //! average values from Andrews, 1986 (ft2 ft-3)
const double sigma_i[] = {surf2vol[0] / cm2ft, surf2vol[1] / cm2ft, surf2vol[2] / cm2ft}; //! here converted to (cm2 cm-3)

////////////////////////////////////////////////////////////////////////////////////////
// FILE SCOPE GLOBAL VARIABLES
bool bavard = true; //! change this to true for diagnostic output from fire subroutine (for debugging)

// Variables calculated in lmfire() and passed to calcROS()
double wn;		 // sum of all live and dead biomass (g m-2)
double rho_b;	 //! weighted average of fuel bulk density (kg m-3)
double sigma;	 //! surface area to volume ratio of the fuel (cm2 cm-3)
double omega_o;	 //! relative daily litter moisture
double relmoist; //! relative moisture content of the fuel relative to its moisture of extinction (unitless; omega_o / m_e)
double Uforward; //! windspeed (m min-1)

// Variables calculated in calcROS() and passed back to lmfire()
double rateofspread; //! fire rate of spread (m min-1)

// Variables calculated in lmfire() and passed to firemortality()
double Isurface;		  // surface fire intensity (kW m-1), aka fireline intensity, energy release (kW) per unit length (m)
double cbht;			  // canopy base height (m)
double cht;				  // canopy height (m)
double cbd;				  // total canopy bulk density; sum bulk density from cbht to cht
double cbdavg;			  // average bulk density from cbht to cht (kg m-3)
double cfm;				  // canopy foliar moisture content (%)
bool criticalROS = false; // fire spread through canopy possible
double tau_l;			  //! fire residence time (min)

// Variables calculated in firemortality() and passed back to lmfire()
bool criticalSH = false; // if scorch height reaches canopy base height
bool crownfire = false;	 // if crown fire occurs, all conditions for initiation and spread must be met


// added by weichao
double Scorch_height;
double Rate_of_spread;
// INPUT/OUTPUT PARAMETERS
//@KE added to PATCH class
// int cumfires;			//!number of fires currently burning
// double totburn; 		// total area burned (ha d-1) over the course of a year
// double precsum; 		// precipitation summed up over the course of a year (mm)

//@KE added to INDIVIDUAL class - @KE adaption for LPJ-GUESS
// Calculated in firemortality() and passed back to lmfire()
// double CK;			// fraction of crown scorch (fraction) for each cohort
// double P_m; 			// total probability of mortality for each cohort
// double P_mCK;			// probability of mortality due to crown kill for each cohort
// double P_mtau; 		// probability of mortality due to cambial damage for each cohort
// Calculated in lmfire()
// double day_kill;		//daily probability of mortality @KE added
// double day_CKkill;	//daily probability of mortality due to crown damage only @KE added
// double day_taukill;	//daily probability of mortality due to cambial damage only @KE added

// OUTPUT PARAMETERS	tracked in global classes and updated in lmfire()
//@KE added to PATCH class
// int anumfires; 		// annual number of fires
// int burndays; 		// annual burndays
// double unburneda; 	//!unburned area (ha)
// double afire_frac;	//!fraction of the gridcell burned
// double sumfdi; 		//!FDI summed up over the course of a year
// double burnedf; 		// LMFire fraction of area burned in gridcell // LPJ-GUESS fraction of patch killed due to fire

// double mnumfires[12];	// number of fires over the course of a month
// double mtotburn[12];	// sum of area burned (ha d-1) over the course of a month
// double rabarf[12];	// sum of raw area burned (ha d-1) before limited by patch size
// double mfdi[12];		// Average FDI over the course of a month
// double mnlig[12];		// number of lightning caused-ignition events per month
// double ROSf[12];		//!forward rate of spread of fire (m min-1) average over the course of a month
// double ROSb[12];		//!backward rate of spread of fire (m min-1) average over the course of a month
// double Isurf[12];		//!surface fire intensity (kW m-1) average over the course of a month

//@KE added to STANDPFT class
// double BBlive;		// sum/mean across patches for burned live fuel by PFT (kgC m-2)
// double fkill;			// sum/mean across patches for tree biomass killed (but not consumed) by fire (kgC m-2) by PFT
// double CKkill;		// Due to crown damage: sum/mean across patches for tree biomass killed (but not consumed) by fire (kgC m-2)
// double taukill;		// Due to cambial damange: sum/mean across patches for tree biomass killed (but not consumed) by fire (kgC m-2)

//@KE added to FLUXES class
// double acflux_fireveg;// annual carbon flux to atmosphere from burnt vegetation only (kgC m-2)
// double acflux_fire;	// annual carbon flux to atmosphere from burnt vegetation and litter (kgC m-2)

///////////////////////////////////////////////////////////////////////////////////////
// RATE OF SPREAD
// Internal function for calculating rate of spread.
//!-----------------------------------------------------------------------------------------------------------------------------------------------------
void calcROS()
{
	// double wn, double rho_b, double sigma, double omega_o, double relmoist, double Uforward, double& rateofspread

	// INPUT PARAMETERS
	// Variables calculated in lmfire() and passed to calcROS()
	//	wn;					//sum of all live and dead biomass (g m-2)
	//	rho_b;          	//!weighted average of fuel bulk density (kg m-3)
	//	sigma;          	//!surface area to volume ratio of the fuel (cm2 cm-3)
	//	omega_o;			//!relative daily litter moisture
	//	relmoist;       	//!relative moisture content of the fuel relative to its moisture of extinction (unitless; omega_o / m_e)
	//	Uforward;      		//!windspeed (m min-1)

	// OUTPUT PARAMETERS
	// Variables calculated in calcROS() and passed back to lmfire()
	//	rateofspread;   	//!fire rate of spread (m min-1)

	//! local parameters
	const double rho_p = 513;	 // 390.; //FireBGS 390. Orginal 513.;		//!oven-dry particle density (kg m-3) Thonicke 2010 BGS
	const double nu_s = 0.41739; //! mineral dampening coefficient (unitless)

	//! local variables
	double A; //! empirical function of sigma, between approx. 0.2 to 1.0, reduces sensitivity of the ratio gammaprime / gammaprimemax for large values of sigma
	double B;
	double C;
	double E;
	double Beta;		  //! packing ratio (unitless) (fuel bulk density / oven dry particle density)
	double Beta_op;		  //! optimum packing ratio (unitless)
	double Gammaprime;	  //! potential reaction velocity; reaction velocity that would occur if fuel were free of moisture and mineral content; = Gamma / nu_M / nu_s
	double Gammaprimemax; //! maximum reaction velocity; would occur if fuel elements in the fuelbed were arranged in the most efficient way possible (min-1)
	double IR;			  //! reaction intensity (kJ m-2 min-1)
	double Phiw;		  //! wind coefficient (?)
	double pratio;		  //! ratio of packing ratio to optimum packing ratio (unitless)
	double Qig;			  //! heat of pre-ignition (kJ kg-1)
	double epsilon;		  //! effective heating number; proportion of fuel that is heated before ignition occurs (dimensionless), between 0 and 1 (1 at infinitive surface to volume ratio))
	double nu_M;		  //! moisture dampening coefficient (unitless)
	double xi;			  //! propagating flux ratio; proportion of IR transferred to unburned fuels (dimensionless); in reality influenced by convection, radiation, flame contact and ignition-point transfer
	double windfact;	  //! high wind multiplier for rate of spread (unitless)
	double Ums;			  //! wind speed (m s-1)

	//	!-------------------------------
	//	!Terms used in several equations

	//	!---
	//	!packing ratio (unitless)

	Beta = rho_b / rho_p;
	// dprintf("Beta %f = rho_b %f / rho_p %f\n",Beta, rho_b, rho_p);

	//	!---
	//	!optimum packing ratio (unitless)

	Beta_op = 0.200395 * pow(sigma, -0.8189);

	pratio = Beta / Beta_op;
	// dprintf("Beta_op %f Beta %f rho_b %f rho_p %f\n",Beta_op, Beta,rho_b,rho_p);

	//	!-------------------------------
	//	!Term 1: reaction intensity

	//	!---
	//	!maximum reaction velocity (min-1)

	Gammaprimemax = 1 / (0.0591 + 2.926 * pow(sigma, -1.5));
	// dprintf("Gammaprimemax %f sigma %f\n",Gammaprimemax, sigma);
	//	!---
	//	!optimum reaction velocity (min-1) Rothermel 1972 INT 115

	A = 8.9033 * pow(sigma, -0.7913);

	Gammaprime = Gammaprimemax * pow(pratio, A) * exp(A * (1 - pratio));
	// dprintf("pratio %f\n",pratio);

	//	!---
	//	!moisture dampening coefficient

	nu_M = 1. - 2.59 * relmoist + 5.11 * pow(relmoist, 2.) - 3.52 * pow(relmoist, 3.);

	//	!---
	//	!reaction intensity

	IR = Gammaprime * wn * h * nu_M * nu_s; //! eqn. A1 (kJ m-2 min-1)
	// dprintf("IR %f = Gammaprime %f * wn %f * h %f * nu_M %f * nu_s %f\n",IR,Gammaprime,wn,h,nu_M,nu_s);

	//	!-------------------------------
	//	!Term 2: 'ratio of propagating flux to reaction intensity' (fraction)

	xi = exp(0.792 + 3.7597 * sqrt(sigma) * (Beta + 0.1)) / (192. + 7.9095 * sigma);

	//	!-------------------------------
	//	!Term 3: wind coefficient (unitless?)

	C = 7.47 * exp(-0.8711 * pow(sigma, 0.55));

	B = 0.15988 * pow(sigma, 0.54);

	E = 0.715 * exp(-0.01094 * sigma);

	Phiw = C * pow((3.281 * Uforward), B) * pow(pratio, -E);
	// dprintf("Phiw %f = C %f Uforward %f B %f pratio %f -E %f E %f \n",Phiw,C,Uforward,B,pratio,-E,E);

	//	!-------------------------------
	//	!Term 4: effective heating number (fraction)

	epsilon = exp(-4.528 / sigma); 

	//	!-------------------------------
	//	!Term 5. heat of pre-ignition (kJ kg-1)

	Qig = 581. + 2594. * omega_o;

	//	!-------------------------------
	//	!Term 6. wind multiplier for high wind conditions
	//	!at a windspeed of 10 m s-2 and above, the calculated ROS will be doubled,
	//	!as the BEHAVE-based ROS is increasingly too low at higher wind speeds (see Morvan et al., 2008, Figure 13)

	Ums = Uforward / 60.;
	
	if (Ums <= 10.)
	{
		windfact = 1. + exp(2. * Ums - 20.);
	}
	else
	{
		windfact = 2.;
	}

	//	!-------------------------------
	//	!Rate of spread (m min-1) //Rothermel 1972

	rateofspread = IR * xi * (1. + Phiw) * windfact / (rho_b * epsilon * Qig);
	Rate_of_spread = rateofspread;
	// if (date.year > 500 && date.day > 300){
	// dprintf("rateofspread = %f IR %f xi %f Phiw %f windfact %f / rho_b %f epsilon %f Qig %f\n",
	// rateofspread,IR,xi,Phiw,windfact,rho_b,epsilon,Qig);
	// dprintf("date.day %d date.year %d wind_speed %f\n",date.day,date.year,Ums);
	// }
} // end function calcROS

///////////////////////////////////////////////////////////////////////////////////////
// FIRE MORTALITY
// Internal function for calculating tree mortality due to crown scorch and cambial damage
//!-----------------------------------------------------------------------------------------------------------------------------------------------------

// move to outside of firemortality() by weichao
	double ScorchH;														   //! scorch height (m)

void firemortality(Patch &patch, Pftlist &pftlist)
{

	//! calculate mortality by fire due to crown scorch and cambial damage
	//! PFTs are divided up into height classes in the individualmod module

	// INPUT PARAMETERS - local or global
	// npft;			//number of pfts
	// indiv.X 			//State variables for average individual of a cohort
	// Isurface;   		//!surface fire intensity (kW m-1)
	// F[] = { 0.094, 0.094, 0.094, 0.094, 0.094, 0.094, 0.094, 0.094, -99, -99, -99 };  //!scorch height parameters
	// tau_l;     		//!fire residence time (min)

	// OUTPUT PARAMETERS
	// indiv.CK;		// fraction of crown scorch (fraction) for each cohort - @KE adaption for LPJ-GUESS
	// indiv.P_mCK;		// probability of mortality due to crown kill for each cohort - @KE adaption for LPJ-GUESS
	// indiv.P_mtau; 	// probability of mortality due to cambial damage for each cohort - @KE adaption for LPJ-GUESS
	// indiv.P_m; 		// total probability of mortality for each cohort - @KE adaption for LPJ-GUESS

	//! local parameters
	double RCK = 1.0; // TESTING! LMFire = 0.5; SPITFIRE = 1.0; 	//real(sp), parameter, dimension(npft) :: //CHANGE this could be species specific
	double p = 3.0;

	//! local variables

	int nclass = min(date.year / estinterval + 1, OUTPUT_MAXAGECLASS); // nhclass  = 7;   //!number of height classes for discretization ////CHANGE this isn't used anywhere...
	double dphen;													   //! phenological state (1=full leaf; 0=no leaves)
	double barkt;													   // bark thickness (cm)

	double CK;	   //! fraction of crown scorch (fraction)
	double tau_c;  //! critical time for cambial damage (min)
	double tau_r;  //! ratio of fire residence time to critical time for cambial damage (ratio)
	double P_mtau; //! probability of mortality due to cambial damage
	double P_mCK;  //! probability of mortality due to crown damage
	double P_m;	   //! total probability of mortality
	int i;		   // for loops 

	// *** Loop through PFTs ***
	pftlist.firstobj();
	while (pftlist.isobj)
	{

		Pft &pft = pftlist.getobj();
		Vegetation &vegetation = patch.vegetation;
		Patchpft &patchpft = patch.pft[pft.id];

		vegetation.firstobj();
		while (vegetation.isobj)
		{
			Individual &indiv = vegetation.getobj();

			// added by weichao
			// dprintf("TESTING pft.id %d indiv.pft.id %d\n",pft.id,indiv.pft.id);

			if ((indiv.pft.lifeform == TREE) && (indiv.pft.id == pft.id))
			{

				//		dprintf("indiv.crownlength %.2f CLs %.2f * indiv.height %.2f + CLi %.2f pft %s\n",
				//				indiv.crownlength,indiv.pft.CLs,indiv.height,indiv.pft.CLi,(char*) pft.name);

				//--------------------------
				//! crown kill by height class

				// calculate scorch height

				ScorchH = F[indiv.pft.id] * pow(Isurface, 0.667); //! scorch height (m) //KE note, Van Wagner 1973 0.148*I^2/3
				Scorch_height = ScorchH;	
				// KE note Van Wagner 1973 CJFR 0.385*I^2/3 mixed oak and pine forests is in kcal/s-m
						// if(date.year-nyear_spinup == 20){
							// dprintf("patch.id = %d managed = %d pft.id = %d ScorchH %.2f, indiv. bole height %.2f, indiv.height %.2f indiv.crownlength %.2f pft %s\n",
									// patch.id, patch.managed, pft.id, ScorchH,(indiv.height-indiv.crownlength),indiv.height,indiv.crownlength,(char*) pft.name);
						// }

				// Check if crown fire possible (LMFIRE_CF only)
				if (firemode == LMFIRE_CF)
				{

					// Check if scorch height reaches canopy base height, set boolean criticalSH @KE crown fire modeling
					if (ScorchH > cbht)
					{
						criticalSH = true;
						//				dprintf("criticalSH = true, cbht = %.2f SH = %.2f\n",cbht,SH);
						//				dprintf("criticalROS = %d ",criticalROS);
						//				dprintf("CBD = %.2f CFM = %.2f date.year %d\n",cbdavg,cfm,date.year);
					}

					else
					{
						criticalSH = false;
						//				dprintf("criticalSH = false, cbht = %.2f SH = %.2f\n",cbht,SH);
					}

					if ((criticalSH == true) && (criticalROS == true))
					{
						crownfire = true;
						CK = 1.0; // total mortality due to crown kill
						// dprintf("CROWN FIRE patch.id %d date.year %d\n",patch.id,date.year);
					}
					else
					{
						crownfire = false;
						// CK = 0.; // TESTING JULY
						// CK = (SH - indiv.height + indiv.crownlength) / indiv.crownlength;
						//! proportion of the crown affected by fire (combusted)
					}
				}

				// firemode == LMFIRE
				else
				{
					// revised by weichao
					
					if (indiv.crownlength > 0.0)
					{
						CK = (ScorchH - indiv.height + indiv.crownlength) / indiv.crownlength;
					}
					else {
						CK = 0.0;
					}
					// end of revision by weichao

					// CK = (SH - indiv.height + indiv.crownlength) / indiv.crownlength;
					//! proportion of the crown affected by fire (combusted)
				}

				CK = max(min(CK, 1.), 0.); //! keep the proportion between 0 and 1

				//		if(date.year > 990){
				//		dprintf("CK %.2f SH %.2f indiv.height %.2f indiv.boleht %.2f, indiv.pft.id %d, indiv.id %d\n",
				//				CK,SH,indiv.height,indiv.boleht,indiv.pft.id,indiv.id);
				//		}

				indiv.CK = CK;

				dphen = indiv.phen;

				P_mCK = RCK * pow(CK, p) * dphen; //! Eqn. 22 probability of mortality due to crown damage
				if(std::isnan(P_mCK)){dprintf("warning--------------P_mCK = %f\n",P_mCK);}
				//	dprintf("P_mCK %.2f = RCK %.2f * pow(CK %.2f, p %.2f) * dphen %.2f indiv.pft.id %d indiv.id %d\n",
				//			P_mCK,RCK,CK,p,dphen,indiv.pft.id,indiv.id);

				indiv.P_mCK = P_mCK;

				//!--------------------------
				//! cambial kill by height class
                // added by weichao
				if (!negligible(indiv.cmass_leaf) && !negligible(indiv.pft.wooddens)) {

			        indiv.height = indiv.cmass_sap / indiv.cmass_leaf / indiv.pft.sla * indiv.pft.k_latosa / indiv.pft.wooddens;
					if(indiv.height > 150.0 || std::isnan(indiv.height)){dprintf("warning--------------indiv.height = %f\n",indiv.height);}

			        // Stem diameter (Eqn 5)
			        indiv.diameter = pow(indiv.height / indiv.pft.k_allom2, 1.0 / indiv.pft.k_allom3);
					if(std::isnan(indiv.diameter)){dprintf("warning--------------indiv.diameter = %f\n",indiv.diameter);}
				}
				else {
					indiv.height = 0.0;
					indiv.diameter = 0.0;
				}
				
				
				//! calculate bark thickness
                
				barkt = indiv.pft.BTs * (indiv.diameter * 100.) + indiv.pft.BTi; //! bark thickness (cm) eqn. 21 //diameter convert m to cm
				indiv.barkt = barkt;
				//		barkt = par1[indiv.pft.id] * (indiv.diameter * 100.) + par2[indiv.pft.id];  //!bark thickness (cm) eqn. 21 //diameter convert m to cm

				tau_c = 2.9 * pow(barkt, 2.); //! critical time for cambial damage (min)

				// revised by weichao
				tau_r = 0.0;
				if (tau_c > 0.0)
				{
					tau_r = tau_l / tau_c; //! tau ratio: fire residence time / critical time for cambial damage
				}

				// tau_r = tau_l / tau_c;			//!tau ratio: fire residence time / critical time for cambial damage
				// end of revision by weichao

				// dprintf("barkt %f diameter %f tau_l %f tau_c %f\n",barkt,(indiv.diameter*100.),tau_l,tau_c);

				if (tau_r >= 2.)
				{
					P_mtau = 1.;
				}
				else
				{
					P_mtau = 0.563 * tau_r - 0.125;
				}
                
				// revised by weichao
				// P_mtau = 0.; // TESTING NEW JULY

				P_mtau = max(0., P_mtau); //! do not allow P_mtau to be less than zero

				indiv.P_mtau = P_mtau;

				//!--------------------------
				//! Eqn. 18 total probability of mortality (per woody PFT and height class)

				indiv.P_m = P_mtau + P_mCK - P_mtau * P_mCK;

				// test by weichao
				if (std::isnan(indiv.P_m))
				{
					dprintf("Attention: Fire probability of mortality problems----------------------------\n");
				}
				// end of test by weichao

				// if (date.year-nyear_spinup == 20 && date.day >240){
				// dprintf("Date_year = %d INSIDE firemortatliy() P_m %.2f = P_mtau %.2f + P_mCK %.2f - P_mtau %.2f * P_mCK %.2f\n",
				// date.year, indiv.P_m, P_mtau, P_mCK, P_mtau, P_mCK);
				// // dprintf("indiv.P_m %.2f indiv.P_mtau %.2f indiv.P_mCK %.2f\n",
				// // indiv.P_m, indiv.P_mtau, indiv.P_mCK);
				// }

			} // end if TREE
			// ... on to next individual
			vegetation.nextobj();
		} // end cohort loop

		// ... on to next PFT
		pftlist.nextobj();
	} // end pft loop
	// if ((criticalSH == true) && (criticalROS == true)) {
	//	dprintf("\nCROWN FIRE = true date.year %d date.day %d\n",date.year,date.day);
	// }
} // end subroutine firemortality

///////////////////////////////////////////////////////////////////////////////////////
// LM FIRE
// Calculates fire ignitions, spread, and effects
// Should be called by framework at the end of each simulation day, after climate and soil attributes have been updated

// REVISIT - PATCH level - use patch loop when calling this function
// void lmfire(Patch& patch, Gridlist& gridlist, Stand& stand, Pftlist& pftlist){
void lmfire(Patch &patch, Stand &stand, Gridcell &gridcell, Pftlist &pftlist)
{ // modified by weichao
    // int count_manage_or_not;
	// if(date.day == 0 && patch.managed == 0){
		// count_manage_or_not = 0;
	// }
	// if(date.year == 510 && date.day > 360){
		// count_manage_or_not +=1;
	    // dprintf("date.day = %d patch.managed = %d stand.id = %d patch.id = %d count_manage_or_not = %d\n",
        // date.day, patch.managed, stand.id, patch.id, count_manage_or_not);
	// }
	// Climate& climate = stand.climate;
	Climate &climate = gridcell.climate; // modified by weichao
										 // if ((date.year - nyear_spinup == 1) && (date.day == 20)){
										 // dprintf("temperature = %f radiation = %f precipitation = %f  wind = %f light = %f NI  = %f\n",climate.temp,climate.rad,climate.prec,climate.wind,climate.lght,climate.NI);

	// }
	////////////////////////////////
	// Declare Variables

	// Counters used
	int i = 0;
	int j = 0;
	int z = 0;
	int p = 0;

	// INPUT PARAMETERS climate.
	// CLIMATE class
	// double prec;		// precipitation today (mm)

	//@KE added to CLIMATE class
	// double magdd20;	// accumulated growing degree days (20-year running mean)
	// double dlght;		// !daily interpolated lightning flashes (ha-1 day-1)
	// double wind;		// !interpolated wind speed today (m s-1)
	// double NI;		// !Nesterov fuel dryness Index (degC^2) for the day
	// calculated from climate in driver.cpp, fuels not involved
	// double tmax;		// maximum temperature today (degC)
	// double esat;		// saturation vapor pressure today (Pa), calculated in driver.cpp

	// INPUT/OUTPUT PARAMETERS - local or global
	// *see list at top of this file

	// OUTPUT PARAMETERS - used in firemortality()
	// *also see list at top of this file

	// Local parameters
	double area_ha = 1.e-4 * firepatch; //! convert m2 to ha; // fire patch area (m2) (default is 1000) set from ins file
	// if ((date.year - nyear_spinup == 1) && (date.day == 1)){
	// dprintf("Year = %d Day = %d: ",date.year);
	// dprintf("firepatch = %f area_ha = %f\n",firepatch,area_ha);

	// }
	double avg_cont_area = area_ha; //! average contiguous area size of non-agricultural part of the gridcell (ha) //CHANGE this assumes NO agricultural land
	int nspec = 6;					//! number of gaseous species emitted by fires //CHANGE next line
	//! emission factors in units g kg-1 (dry matter)
	// double emfact[npft][nspec] = reshape([emCO2, emCO, emCH4, emVOC, emTPM, emNOx ],[npft,nspec] );

	// Local variables
	double fpc_patch;  // FPC of trees for patch
	double ieff_patch; // ignition efficiency for patch
	double ieff_avg;   // average pft ieff
	double totbiomass; // total biomass, this was gridcell area in LMFire
	if (date.day == 0)
	{						// initialize
		patch.cumfires = 0; // int
		patch.burnedf = 0.;
		patch.totburn = 0.;
		
	    patch.annual_fire_flux = 0.0;
        patch.annual_fire_fuel_flux = 0.0;
	    patch.annual_fire_veg_flux = 0.0;
		
		patch.annual_killed_veg_flux = 0.0;
		
	    patch.annual_fire_flux_confirm = 0.0;
        patch.annual_fire_fuel_flux_confirm = 0.0;
	    patch.annual_fire_veg_flux_confirm = 0.0;
		
		patch.omega_o0 = 0.0;
		patch.omega_o = 0.0;
	}
	if (date.dayofmonth == 0)
	{ // initialize
		patch.mtotburn[date.month] = 0.;
		patch.rabarf[date.month] = 0.;
		patch.mnlig[date.month] = 0.;
		patch.ROSb[date.month] = 0.;
		patch.ROSf[date.month] = 0.;
		patch.Isurf[date.month] = 0;
	}

	double ieff; // ignition efficiency for gridcell
	double FDI;	 // fire danger index

	double grasscover = 0.; // sum indiv.fpc for GRASS and CROP pfts
	double treecover = 0.;	// sum indiv.fpc for TREE pfts
	double totvcover = 0.;	// sum indiv.fpc

	double deadfuel[npft][4]; //! dead fuel load in 1- 10- 100- and 1000- hour fuel classes (g DM m-2) //Note in g
	double livefuel[npft][4]; //! live fuel load in 1- 10- 100- and 1000- hour fuel classes (g DM m-2)

	// dprintf("npft %d\n",npft);

	for (i = 0; i < npft; i++)
	{ // initialize
		for (j = 0; j < 4; j++)
		{
			deadfuel[i][j] = 0.;
			livefuel[i][j] = 0.;
		}
	}

	double light; //! frequency of total lighting flashes (ha-1 d-1)
	double Ukmh;  //! wind speed in km h-1
	double Ustar; //! day mean wind speed (m s-1)

	/////////////////////////////////////////////////////////////////////////////////////////
	// FIRE IGNITIONS
	// Calculates lightning and human ignition events - CHANGE add human events!

	// Lightning Ignition Events
	// efficiency of lightning ignition decreases with increasing burned area
	// calculate weighted average ignition efficiency

	/////////////////////////////////////////////////////////////////////////////////////////
	//!--------------------------------------------------------------------------------
	// !part 2.2.1, ignition events
	// !lightning
	// !efficiency of lightning ignition decreases with increasing burned area

	// light = stand.climate.dlght * 0.01; //!convert from km-2 to ha-1 // (flashes ha-1 day-1)
	// modified by weichao
	// light = gridcell.climate.dlght * 0.01; //!convert from km-2 to ha-1 // (flashes ha-1 day-1)
	light = gridcell.climate.lght * 0.01; //! convert from km-2 to ha-1 // (flashes ha-1 day-1)

	// dprintf("lightning strikes stand.climate.dlght %f light %f date.day %d date.month %d date.year %d\n",
	//		stand.climate.dlght,light,date.day,date.month,date.year);

	// Loop through cohorts/individuals to calculate ieff_patch & totbiomass

	// Reset to zero before calculating for the time step
	i = 0;
	j = 0;
	fpc_patch = 0.0;
	ieff_patch = 0.0;
	totbiomass = 0.0;

	// Obtain reference to Vegetation object for this patch
	Vegetation &vegetation = patch.vegetation;

	// TESTING AUGUST - starting after spinup with no litter to build up
	//  if (date.year - nyear_spinup == 100 && date.day == 240){
	//  for (i=0; i<npft; i++){

	// dprintf("BEFORE patch.pft[i].litter_leaf %f  patch.pft[i].litter_root %f  patch.pft[i].litter_repr %f  patch.pft[i].litter_wood %f\n",
	// patch.pft[i].litter_leaf, patch.pft[i].litter_root, patch.pft[i].litter_repr, patch.pft[i].litter_wood);

	// patch.pft[i].litter_leaf = 0.;
	// patch.pft[i].litter_wood = 0.;
	// patch.pft[i].litter_root = 0.;
	// patch.pft[i].litter_repr = 0.;

	// dprintf("AFTER patch.pft[i].litter_leaf %f patch.pft[i].litter_wood %f i %d \n",patch.pft[i].litter_leaf,patch.pft[i].litter_wood,i);
	// }
	// }

	vegetation.firstobj();
	while (vegetation.isobj)
	{
		Individual &indiv = vegetation.getobj();

		// Pft& pft=pftlist.getobj();
		// if ((date.year - nyear_spinup > 80) && date.islastday && date.islastmonth){
		// dprintf("year = %d  month = %d  day = %d  ismanaged = %d  patchID = %d  pft.id = %d  litter_leaf = %f  litter_root = %f  litter_wood = %f  litter_sap = %f litter_heart = %f\n", date.year,date.month,date.day,
		// patch.managed, patch.id, pft.id, patch.pft[indiv.pft.id].litter_leaf, patch.pft[indiv.pft.id].litter_root, patch.pft[indiv.pft.id].litter_wood, patch.pft[indiv.pft.id].litter_sap, patch.pft[indiv.pft.id].litter_heart);
		// // dprintf("pft.id = %f\n",pft.id);
		// }

		// if (std::isnan(indiv.fpc)){
		// dprintf("date.day = %d date.year = %d\n",date.day,date.year); // added by weichao
		// }

		// added by weichao
		// Pft& pft=pftlist.getobj();
		// if(indiv.pft.id==pft.id){
		// patch.pft[pft.id].litter_leaf = patch.pft[indiv.pft.id].litter_leaf;
		// patch.pft[pft.id].litter_root = patch.pft[indiv.pft.id].litter_root;
		// patch.pft[pft.id].litter_wood = patch.pft[indiv.pft.id].litter_wood;
		// patch.pft[pft.id].litter_sap = patch.pft[indiv.pft.id].litter_sap;
		// patch.pft[pft.id].litter_heart = patch.pft[indiv.pft.id].litter_heart;
		// }

		// if ((date.year - nyear_spinup == 10) && (date.day == 30)){
		// dprintf("indiv.cmass_leaf = %f indiv.fpc = %f\n",indiv.cmass_leaf,indiv.fpc);
		// }
        
		fpc_patch += indiv.fpc;
		ieff_patch += (indiv.fpc * ieffpft[i]); //@KE later add to ins file and CHANGE ieffpft[i] to indiv.pft.ieff

		totvcover += indiv.fpc;

		if ((indiv.id != -1) && indiv.alive)
		{
			totbiomass += indiv.cmass_leaf + indiv.cmass_root + indiv.cmass_sap + indiv.cmass_heart - indiv.cmass_debt;
		} // alive?

		// Sum grasscover
		if (indiv.pft.lifeform == GRASS || indiv.pft.lifeform == CROP)
		{
			grasscover += indiv.fpc;
		}
		// Sum treecover
		else if (indiv.pft.lifeform == TREE)
		{
			treecover += indiv.fpc;
		}

		i++;
		// ... on to next individual
		vegetation.nextobj();
	}
	treecover = min(treecover, 0.95); // added by weichao
	grasscover = min(grasscover, 1.0); // added by weichao
	// if(date.year-nyear_spinup == 20){
		// dprintf("treecover = %f grasscover = %f\n", treecover, grasscover);
	// }

	// Calculate weighted average of ignition efficiency
	if (fpc_patch != 0)
	{
		ieff_avg = ieff_patch / fpc_patch;
		// if ((date.year - nyear_spinup == 10) && (date.day == 30)){
		// dprintf("ieff_avg = %f  , ieff_patch = %f  , fpc_patch = %f\n",ieff_avg,ieff_patch,fpc_patch);
		// }
	}
	else
	{
		ieff_avg = 0.;
	}

	/////////////////////////////////////////////////////////////////////////////////////////
	//!--------------------------------------------------------------------------------
	//! part 2.2.2, fuel moisture content

	// SATURATION VAPOR PRESSURE (esat) is calculated in esat() defined in driver.cpp and
	// called in this function to calculate dewpoint

	// NESTEROV INDEX is calculated in interp_climate() defined in driver.cpp and
	// called by readclm_ncdf() or readclm() in getenv.cpp

	// double patch.omega0[4]; //! moisture content of each fuel class on the previous day
	if (date.day == 0)
	{ // initialize
		for (i = 0; i < 4; i++)
		{
			patch.omega0[i] = 0.0;
			// patch.omega0[i] = 1.0; // modified by weichao, set fuel moisture to fully wet on first day
		}
	}
	double omega[4]; //! moisture content of each fuel class
	double omega_s1; //! top layer soil water content
	double omega_lg; //! relative moisture content of live grass //used to calc omega_nl
	double omega_nl; //! mean relative moisture contents of 1-h fuel class and live grass
	double rm;		 //! moisture relative to moisture of extinction (omega_o / m_e)
	double me_nl;	 //! mass weighted average moisture of extinction for live grass and 1hr fuel //!live grass fuel moisture content
	// deleted by weichao
	// double SOM_surf; 	//!mass of the total organic matter in the surface soil C pool
	// double cpool_surf; 	//!surface SOM pool (2yr turnover time) (g C m-2)
	// cpool_surf = 0.;// initialize each day
	// end of deletion by weichao
	double wlivegrass; // mass of live grass (g m-2)
	wlivegrass = 0.;   // initialize each day
	double wfinefuel;  //! mass of fine fuels
	// double alpha[4] = {1.e-3, 5.424e-5, 1.485e-5, 1.e-6};  //!drying parameter for 1- 10- 100-, and 1000-h fuel classes (degC-2)
	double alpha[4] = {1.e-3, 5.424e-5, 1.485e-5, 1.e-6}; //! drying parameter for 1- 10- 100-, and 1000-h fuel classes (degC-2)

	for (i = 0; i < 4; i++)
	{
		// alpha[i] *= 1; //1.5 TESTING JULY
		alpha[i] *= 1; // modified by weichao
	}
	double alpha_lg; // drying parameter for live grass
	double wet;
	double dry;

	//!=====new water balance approach====
	//!---
	//! calculate dewpoint
	//! To estimate dewpoint temperature we use the day's minimum temperature
	//! this makes the assumption that there is a close correlation between Tmin and dewpoint
	//! see, e.g., Glassy & Running, Ecological Applications, 1994

	double es; //! saturation vapor pressure (mbar)
	double tdew;

	// esat(stand.climate);
	esat(gridcell.climate); // modified by weichao

	// es = 0.01 * stand.climate.esat; //!saturation vapor pressure (mbar), convert Pa to mbar
	es = 0.01 * gridcell.climate.esat; // modified by weichao

	tdew = 34.07 + 4157. / log(2.1718e8 / es); //! Josey et al., Eqn. 10 (K)

	//!---
	//! convert calculated temperatures from K to degC
	tdew = tdew - 273.15;

	// if(date.day == 240){
	// dprintf(" date.day %d climate.esat %f climate.tmax %f tdew %f \n",date.day, gridcell.climate.esat, gridcell.climate.tmax, tdew);
	// }
	// wet =  min((stand.climate.prec / 50.), 1.0);
	wet = min((gridcell.climate.prec / 50.), 1.0); // modified by weichao

	for (i = 0; i < 4; i++)
	{

		// dry = stand.climate.tmax * (stand.climate.tmax - tdew) * alpha[i] * patch.omega0[i];
		patch.omega0[i] = min(max(patch.omega0[i], 0.), 1.);											 // added by weichao due to huge wrong number showed in omega0
		dry = gridcell.climate.tmax * (gridcell.climate.tmax - tdew) * alpha[i] * patch.omega0[i]; // modified by weichao
		// dry = min(max(dry, 0.),1.); // added by weichao due to negative values
		// dry = gridcell.climate.tmax * (gridcell.climate.tmax - gridcell.climate.tmin-4.0) * alpha[i] * patch.omega0[i]; // modified by weichao
		omega[i] = min(max(patch.omega0[i] - dry + wet, 0.), 1.);

		// dprintf("omega[%d] = (%f)  patch.omega0[%d] = (%f)  dry = %f  wet = %f\n", i, omega[i], i,patch.omega0[i],dry,wet); //@KE added print

		patch.omega0[i] = omega[i];

		// if(i ==1 && date.year == 600){
		// dprintf("date.day %d omega[%d] = (%f)  patch.omega0[%d] = (%f)  alpha[%d] =(%f) dry = %f  wet = %f\n", date.day, i, omega[i], i,patch.omega0[i],i,alpha[i],dry,wet); //@KE added print
		// }
	}

	//! transfer remaining fast litter to surface SOM pool with turnover time of klit2som
	// deleted by weichao
	// double dt = 1./12.;  //!timestep
	// double klit2som = 1. - exp(-2.);  //!transfer rate for fast litter to surface SOM pool
	// double lit2som[npft];         //!amount of C transferred from fast litter to surface SOM pool

	// // added by weichao

	// for (p = 0; p < npft; p++) {
	// lit2som[p] = dt * klit2som * patch.pft[p].litter_leaf;  //!amount of litter to transfer to soil (vector over PFTs)
	// patch.pft[p].litter_leaf = max(patch.pft[p].litter_leaf - lit2som[p], 0.); //@KE CHANGE or check, updates litter
	// cpool_surf += lit2som[p];  //!sum across all pfts

	// if ((date.year - nyear_spinup == 10) && (date.day > 270)){
	// dprintf("patch.pft[%d].litter_leaf = %f lit2som[%d] = %f\n",p,patch.pft[p].litter_leaf,p, lit2som[p]);
	// // dprintf("pft.id = %f\n",pft.id);
	// }

	// }
	// if ((date.year - nyear_spinup == 10) && (date.day > 270)){
	// dprintf("cpool_surf = %f\n",cpool_surf);
	// // dprintf("pft.id = %f\n",pft.id);
	// }

	// //!mass of the total organic matter in the surface soil C pool
	// SOM_surf = cpool_surf * c2om * 1000.; //Convert kg to g
	// //dprintf("SOM_surf %f = cpool_surf %f * c2om * 1000\n",SOM_surf,cpool_surf,c2om);
	// end of deletion by weichao

	//! top layer soil water content
	omega_s1 = patch.soil.dwcontupper[date.day]; // daily water content in upper soil layer for each day of year
	const double b = 10. / 9.;					 // from LMFire
	const double c = 1. / 9.;					 // from LMFire

	// Calculation of relative moisture content of live grass (omega_lg)
	omega_lg = max(0., b * omega_s1 - c);
	// if(date.islastday && date.islastmonth){
	// dprintf("omega_lg = (%f)\n", omega_lg); //@KE added print
	// }

	// Calculation of deadfuel !dead fuel load in 1- 10- 100- and 1000- hour fuel classes (g DM m-2)
	double woi[4]; //! 1-, 10-, 100- and 1000-h dead fuel mass summed across all PFTs (g m-2)
	for (i = 0; i < 4; i++)
	{
		// woi[0] = 0.0; // maybe this is wrong
		woi[i] = 0.0; // revised by weichao
	}

	// *** Loop through PFTs ***
	pftlist.firstobj();
	while (pftlist.isobj)
	{

		Pft &pft = pftlist.getobj();
		Standpft &standpft = stand.pft[pft.id];

		// Partitioning more to 1 hour fuels - 6/2/19 TEST
		//  5% increase to 1 hr * 0.04725, reducing 1000 hour * 0.66775
		//	deadfuel[pft.id][0] = c2om * (0.04725 * (patch.pft[pft.id].litter_wood
		//			+ patch.pft[pft.id].litter_repr) + patch.pft[pft.id].litter_leaf) * 1000.;  //!1-h fuel class *1000 converts kg to g
		//	deadfuel[pft.id][1] = c2om * 0.075 * patch.pft[pft.id].litter_wood * 1000.;               //!10-h fuel class
		//	deadfuel[pft.id][2] = c2om * 0.21 * patch.pft[pft.id].litter_wood * 1000.;              //!100-h fuel class
		//	deadfuel[pft.id][3] = c2om * 0.66775 * patch.pft[pft.id].litter_wood * 1000.;              //!1000-h fuel class

		// 10% increase to 1 hr * 0.04725, reducing 1000 hour * 0.66775

		// added by weichao
		// patch.pft[pft.id].litter_wood = patch.pft[pft.id].litter_sap + patch.pft[pft.id].litter_heart;

		// // added by weichao
		// double litter_ag;
		// int p;
		// for(p= 0;p<npft;p++){
		// patch.pft[p].litter_wood = 0.0;
		// }
		// for (p=0;p<npft;p++) {
		// if(!std::isnan(patch.soil.sompool[SURFFWD].cmass) && !std::isnan(patch.soil.sompool[SURFCWD].cmass) ){
		// // patch.pft[p].litter_wood += patch.soil.sompool[SURFMETA].cmass/npft; // added by weichao
		// // patch.pft[p].litter_wood += patch.soil.sompool[SURFSTRUCT].cmass/npft; // added by weichao
		// patch.pft[p].litter_wood += patch.soil.sompool[SURFFWD].cmass/npft; // added by weichao
		// patch.pft[p].litter_wood += patch.soil.sompool[SURFCWD].cmass/npft; // added by weichao
		// }

		// // litter_ag += patch.pft[p].litter_leaf + patch.pft[p].litter_sap + patch.pft[p].litter_heart + patch.pft[p].litter_repr;
		// patch.pft[p].litter_wood += patch.pft[p].litter_sap + patch.pft[p].litter_heart;

		// if (date.year - nyear_spinup == 10 && date.month == 8 && date.day == 10){
		// dprintf("year = %d  month = %d  day = %d  ismanaged = %d  patchID = %d  pft.id = %d  litter_leaf = %f  litter_sap = %f litter_heart = %f  litter_wood = %f\n", date.year, date.month, date.day,
		// patch.managed, patch.id, pft.id, patch.pft[p].litter_leaf, patch.pft[p].litter_sap,  patch.pft[p].litter_heart, patch.pft[p].litter_wood);
		// }
		// }
		// added by weichao

		// end of adding by weichao

		// added by weichao
		// if(std::isnan(leaf_littter)){
		// leaf_littter = 0.0;
		// }
		// if(std::isnan(root_littter)){
		// root_littter = 0.0;
		// }
		// if(std::isnan(wood_littter)){
		// wood_littter = 0.0;
		// }

		// patch.pft[pft.id].litter_leaf += leaf_littter/10;
		// patch.pft[pft.id].litter_root += root_littter/10;

		// patch.pft[pft.id].litter_wood += wood_littter/10;

		// Individual& indiv = vegetation.getobj();// added by weichao
		// patch.pft[pft.id].litter_wood += 0.05*indiv.cmass_sap;// added by weichao

		// if(std::isnan(patch.pft[pft.id].litter_wood) || patch.pft[pft.id].litter_wood <= 0.0){
		// patch.pft[pft.id].litter_wood = 0.00001; // added by weichao
		// }

		// if(patch.pft[pft.id].litter_wood > 0){
		// patch.pft[pft.id].litter_wood ;
		// }
		// Patchpft& patchpft=patch.pft[pft.id]; // added by weichao
		// if (date.year - nyear_spinup == 100 && date.day == 0){
		// dprintf("year = %d  month = %d  day = %d  ismanaged = %d  patchID = %d  pft.id = %d  litter_leaf = %f  litter_root = %f  litter_wood = %f  litter_sap = %f litter_heart = %f\n", date.year, date.month, date.day,
		// patch.managed, patch.id, pft.id, patch.pft[pft.id].litter_leaf, patch.pft[pft.id].litter_root, patch.pft[pft.id].litter_wood, patch.pft[pft.id].litter_sap, patch.pft[pft.id].litter_heart);
		// // dprintf("pft.id = %f\n",pft.id);
		// }

		// add some part of soil to litter_wood and litter_leaf
		// patch.pft[pft.id].litter_wood = (patch.pft[pft.id].litter_sap + patch.pft[pft.id].litter_heart + patch.soil.sompool[SURFFWD].cmass/npft + patch.soil.sompool[SURFCWD].cmass/npft);
		// patch.pft[pft.id].litter_leaf = (patch.pft[pft.id].litter_leaf + patch.soil.sompool[SURFMETA].cmass/npft + patch.soil.sompool[SURFSTRUCT].cmass/npft);

		deadfuel[pft.id][0] = c2om * (0.0495 * ((patch.pft[pft.id].litter_sap + patch.pft[pft.id].litter_heart + patch.soil.sompool[SURFFWD].cmass / npft + patch.soil.sompool[SURFCWD].cmass / npft) + patch.pft[pft.id].litter_repr) + (patch.pft[pft.id].litter_leaf + patch.soil.sompool[SURFMETA].cmass / npft + patch.soil.sompool[SURFSTRUCT].cmass / npft)) * 1000.; //! 1-h fuel class *1000 converts kg to g
		deadfuel[pft.id][1] = c2om * 0.075 * (patch.pft[pft.id].litter_sap + patch.pft[pft.id].litter_heart + patch.soil.sompool[SURFFWD].cmass / npft + patch.soil.sompool[SURFCWD].cmass / npft) * 1000.;																																									 //! 10-h fuel class
		deadfuel[pft.id][2] = c2om * 0.21 * (patch.pft[pft.id].litter_sap + patch.pft[pft.id].litter_heart + patch.soil.sompool[SURFFWD].cmass / npft + patch.soil.sompool[SURFCWD].cmass / npft) * 1000.;																																									 //! 100-h fuel class
		deadfuel[pft.id][3] = c2om * 0.6655 * (patch.pft[pft.id].litter_sap + patch.pft[pft.id].litter_heart + patch.soil.sompool[SURFFWD].cmass / npft + patch.soil.sompool[SURFCWD].cmass / npft) * 1000.;																																								 //! 1000-h fuel class

		//	deadfuel[pft.id][0] = c2om * (0.045 * (patch.pft[pft.id].litter_wood
		//			+ patch.pft[pft.id].litter_repr) + patch.pft[pft.id].litter_leaf) * 1000.;  //!1-h fuel class *1000 converts kg to g
		//	deadfuel[pft.id][1] = c2om * 0.075 * patch.pft[pft.id].litter_wood * 1000.;               //!10-h fuel class
		//	deadfuel[pft.id][2] = c2om * 0.21 * patch.pft[pft.id].litter_wood * 1000.;              //!100-h fuel class
		//	deadfuel[pft.id][3] = c2om * 0.67 * patch.pft[pft.id].litter_wood * 1000.;              //!1000-h fuel class

		woi[0] += deadfuel[pft.id][0]; //! 1-h fuel class summed across all PFTs (g dry biomass m-2)
		woi[1] += deadfuel[pft.id][1]; //! 10-h fuel class summed across all PFTs (g dry biomass m-2)
		woi[2] += deadfuel[pft.id][2]; //! 100-h fuel class summed across all PFTs (g dry biomass m-2)
		woi[3] += deadfuel[pft.id][3]; //! 1000-h fuel class summed across all PFTs (g dry biomass m-2)

		// ... on to next PFT
		pftlist.nextobj();
	} // end pft loop

	// if (woi[2] != 0.0){
	//	dprintf("woi[2] %f\n",woi[2]);
	// }

	// Calculation of livefuel !live fuel load in 1- 10- 100- and 1000- hour fuel classes (g DM m-2)

	// nind		!gridcell individual density (indiv/m2) indiv.densindiv
	// hm_ind	!heartwood mass, average individual (g C)	// heartwood C biomass on modelled area basis (kgC/m2) patch.pft[p].cmass_heart
	// sm_ind    !sapwood mass, average individual (g C) // sapwood C biomass on modelled area basis (kgC/m2) patch.pft[p].cmass_sap
	// lm_ind          !leaf mass, average individual (g C) 	// leaf C biomass on modelled area basis (kgC/m2) patch.pft[p].cmass_leaf
	// dphen     !phenological state (1=full leaf; 0=no leaves) per pft, on this day 	double phen;
	// vegetation phenological state (fraction of potential leaf cover)

	// *** Loop through PFTs ***
	pftlist.firstobj();
	while (pftlist.isobj)
	{

		Pft &pft = pftlist.getobj();
		Vegetation &vegetation = patch.vegetation;
		Patchpft &patchpft = patch.pft[pft.id];
		Standpft &standpft = stand.pft[pft.id];

		vegetation.firstobj();
		while (vegetation.isobj)
		{
			Individual &indiv = vegetation.getobj();

			if (indiv.pft.id == pft.id)
			{

				// Partitioning more to 1 hour fuels - 6/2/19 TEST
				//  5% increase to 1 hr from 0.045 to * 0.04725, reducing 1000 hour * 0.66775
				livefuel[indiv.pft.id][0] += c2om * (0.04725 * (indiv.cmass_heart + indiv.cmass_sap) + indiv.cmass_leaf) * 1000.; //! * dphen); !1-h fuel class *1000 converts kg to g
				livefuel[indiv.pft.id][1] += c2om * (0.075 * (indiv.cmass_heart + indiv.cmass_sap)) * 1000.;			   //! 10-h fuel class
				livefuel[indiv.pft.id][2] += c2om * (0.21 * (indiv.cmass_heart + indiv.cmass_sap)) * 1000.;				   //! 100-h fuel class
				livefuel[indiv.pft.id][3] += c2om * (0.66775 * (indiv.cmass_heart + indiv.cmass_sap)) * 1000.;			   //! 1000-h fuel class

				//! mass of live grass
				if (indiv.pft.lifeform == GRASS)
				{
					wlivegrass += c2om * indiv.cmass_leaf * 1000.;
				}
			}

			// ... on to next individual
			vegetation.nextobj();
		} // end cohort loop

		// ... on to next PFT
		pftlist.nextobj();
	} // end pft loop

	// dprintf("livefuel[0] 1 %f 10 %f 100 %f 1000 %f date.year %d\n",
	//		livefuel[0][0]/1000, livefuel[0][1]/1000, livefuel[0][2]/1000, livefuel[0][3]/1000, date.year); //@KE added print

	//! back calculate alpha_lg from omega_lg

	// if ((omega_lg > 0.0) && (stand.climate.NI > 0.0)) {

	if ((omega_lg > 0.0) && (gridcell.climate.NI > 0.0))
	{ // modified by weichao

		// dprintf("omega_lg %f NI %f date.day %d date.year %d\n",omega_lg, stand.climate.NI, date.day, date.year);
		//  alpha_lg = (-log(omega_lg) / stand.climate.NI);
		alpha_lg = (-log(omega_lg) / gridcell.climate.NI); // modified by weichao
	}
	else
	{
		alpha_lg = 0.0;
	}

	// Calculation of mean relative moisture contents of 1-h fuel class and live grass (omega_nl)
	//! moisture content weighted among live grass and 1-h fuel

	wfinefuel = woi[0] + wlivegrass;

	// revised by weichao
	omega_nl = (omega[0] * woi[0] + omega_lg * wlivegrass) / wfinefuel;
	// omega_nl = (omega[0] * woi[0] + omega_lg * (wlivegrass + SOM_surf)) / (wfinefuel + SOM_surf);
	// end of revision by weichao

	if (std::isnan(omega_nl) == 1)
	{
		omega_nl = 0.0; // added by weichao due to nan value
	}
	
	// if(date.islastday && date.islastmonth){
	// dprintf("omega_nl = (%f) ", omega_nl); //@KE added print
	// dprintf("omega[0] %f woi[0] %f omega_lg %f ", omega[0], woi[0], omega_lg); //@KE added print
	// dprintf("wlivegrass %f SOM_surf %f wfinefuel %f \n", wlivegrass,SOM_surf, wfinefuel); //@KE added print
	// }

	//!---
	//! weighted average estimate of fuel relative moisture content
	double wo;	 //! total mass of dead fuel summed across 1- 10- and 100-h fuel classes (g m-2)
	double wtot; // total mass dead fuel + grass
	double rdf;	 //! dead fuel : total fuel
	double rlf;	 //! live fuel : total fuel
	double caf;	 //! mass weighted average alpha for live and dead fuels combined
	double alpha_df;
	double dry_o;
	// double omega_o0;
	if (date.day == 0)
	{
		patch.omega_o0 = 0.0; // CHECK not initialized in spitfiremod.f90, set to 1 here because below omega_o = min(max(patch.omega_o0 - dry_o + wet,0.),1.);
		// could change to file scope variable so line below only happens ln 753
	}
	else
	{
		patch.omega_o0 = patch.omega_o;
	}
	// dprintf("patch.patch.omega_o0 %f\n",patch.patch.omega_o0);
	//  double me_fc[4] = { 0.404, 0.487, 0.525, 0.544 };	//!moisture of extinction for 1- 10- 100-, and 1000-h fuel classes (-)
	// double me_fc[4] = { 0.3, 0.3, 0.3, 0.3 };	//!moisture of extinction for 1- 10- 100-, and 1000-h fuel classes (-) TESTING
	double me_fc[4] = {0.2, 0.2, 0.2, 0.2}; //! moisture of extinction for 1- 10- 100-, and 1000-h fuel classes (-) TESTING
	// double me_fc[4] = { 0.202, 0.2435, 0.2625, 0.272 };	//!moisture of extinction for 1- 10- 100-, and 1000-h fuel classes (-)

	wo = woi[0] + woi[1] + woi[2];
	wtot = wo + wlivegrass;

	rdf = wo / wtot;		 //! dead fuel : total fuel
	rlf = wlivegrass / wtot; //! live fuel : total fuel

	alpha_df = 0.; // initialize

	for (i = 0; i < 3; i++)
	{
		alpha_df += alpha[i] * woi[i];
		//	if(date.islastday && date.islastmonth){
		//		dprintf("(alpha[i] %f * woi[i] %f = alpha_df %f)\n",alpha[i],woi[i],alpha_df);
		//	}
		// dprintf("woi %f g/cm2",(woi[i]*0.0001));
	}
	// dprintf("\n");

	alpha_df /= wo; //! mass weighted average alpha for dead fuel
	if(std::isnan(alpha_df)){alpha_df = 0.0;}

	// if(date.islastday && date.islastmonth){
	// dprintf("final alpha_df = (%f)\n", alpha_df); //@KE added print
	// dprintf("woi[0] = (%f)\n", woi[0]); //@KE added print
	// dprintf("woi[1] = (%f)\n", woi[1]); //@KE added print
	// dprintf("woi[2] = (%f)\n", woi[2]); //@KE added print
	// dprintf("wo = (%f)\n", wo); //@KE added print
	// dprintf("date.year = %d  wtot = (%f)\n", date.year, wtot); //@KE added print
	// dprintf("rdf = (%f)\n", rdf); //@KE added print
	// dprintf("rlf = (%f)\n", rlf); //@KE added print
	// }

	caf = alpha_df * rdf + alpha_lg * rlf; //! mass weighted average alpha for live and dead fuels combined
	// dprintf("caf %f = alpha_df %f * rdf %f + alpha_lg %f * rlf %f \n",caf,alpha_df, rdf, alpha_lg, rlf);

	// tmax !24 hour maximum temperature (C) tdew !dewpoint temperature (C)

	// dry_o = stand.climate.tmax * (stand.climate.tmax - tdew) * caf * patch.patch.omega_o0;
	dry_o = gridcell.climate.tmax * (gridcell.climate.tmax - tdew) * caf * patch.omega_o0; // modified by weichao

	// dprintf("dry_o %f = stand.climate.tmax %f * (stand.climate.tmax %f - tdew %f) * caf %f * patch.omega_o0 %f\n",
	//		dry_o,stand.climate.tmax,stand.climate.tmax,tdew,caf,patch.omega_o0);

	// so drying does not wet!
	dry_o = max(0., dry_o);
	// dprintf("dry_o %f \n",dry_o);

	patch.omega_o = min(max(patch.omega_o0 - dry_o + wet, 0.), 1.);
	// patch.omega_o = patch.omega_o;
	// if(date.year-nyear_spinup > 20 && date.day == 240){
	   // dprintf("patch.omega_o %f = dry_o %f wet %f patch.omega_o0 %f date.day %d\n",patch.omega_o,dry_o, wet, patch.omega_o0, date.day);
	// }

	double sum_woi = 0.0;
	double sum_woi_me_fc = 0.0;
	double m_e;
	const double me_lf = 0.2; //! moisture of extinction for live grass fuels:
							  // fractional moisture above which fuel does not burn (fraction) (20%)
	// const double me_lf  =  0.15;       //!moisture of extinction for live grass fuels: TESTING!
	double me_avg; // !mass weighted average moisture of extinction for all fuels (dead + live grass)

	for (i = 0; i < 4; i++)
	{
		sum_woi += woi[i];
		sum_woi_me_fc += (woi[i] * me_fc[i]);
	}
	m_e = sum_woi_me_fc / sum_woi;

	me_avg = m_e * rdf + me_lf * rlf; // !mass weighted average moisture of extinction for all fuels (dead + live grass)

	// revised by weichao
	me_nl = (me_fc[0] * woi[0] + me_lf * wlivegrass) / wfinefuel;
	// me_nl = (me_fc[0] * woi[0] + me_lf * (wlivegrass + SOM_surf)) / (wfinefuel + SOM_surf);
	// end of revision by weichao

	if (std::isnan(me_nl) == 1)
	{
		me_nl = 0.0; // added by weichao due to nan value
	}

	if (me_avg == 0)
	{
		dprintf("me_problem m_e %f rdf %f me_lf %f rlf %f", m_e, rdf, me_lf, rlf);
	}

	/////////////////////////////////////////////////////////////////////////////////////////
	//!--------------------------------------------------------------------------------
	//! part 2.2.3, fire danger

	// Calculation of FDI
	if (grasscover >= 0.6)
	{											//! BURNING BEHAVIOR DOMINATED BY GRASS CHARACTERISTICS
		FDI = max(0., (1. - omega_nl / me_nl)); //! eqn. 8
	}
	else
	{
		FDI = max(0., (1. - patch.omega_o / me_avg)); //! eqn. 8
	}

	// TESTING AUGUST
	FDI = pow(FDI, 2); // 2 Aug 3 Sept
	
	// if (FDI > 0){
	//	dprintf("FDI %f patch.omega_o %f \n",FDI,patch.omega_o);
	// }
	//---------------------------
	//! lightning ignitions
	int nlig; //! frequency of lightning-caused ignition events (ha-1 d-1) CHANGE really only 0 or 1 from code

	if (light * area_ha > 0.)
	{

		// ieff = pow(FDI,2) * 1.0 * (1. - patch.burnedf) / (1. + 25. * patch.burnedf) * ieff_avg; // TESTING AUGUST
		ieff = FDI * 1.0 * (1. - patch.burnedf) / (1. + 25. * patch.burnedf) * ieff_avg;

		// double prob = randfrac();  //!random value from (0,1)

		// modified by weichao
		double prob = randfrac(stand.seed); //! random value from (0,1)
		
		//! the following condition due to super high fire probability and lower the biomass and litter
		if(date.year-nyear_spinup < 1){
			prob = 0.9;
		}
		else if(date.year-nyear_spinup >= 1){
			prob = 0.9;
		}


			// if(date.year-nyear_spinup == 30 && FDI != 0){
				// dprintf("ieff %f = FDI %f * (1-burnedf %f) / 1 + 25 * burnedf %f * ieff_avg %f\n",
						// ieff,FDI,patch.burnedf,patch.burnedf,ieff_avg);
			 	// dprintf("FDI %f = ", FDI);
			  	// dprintf("1 - patch.omega_o %f / ", patch.omega_o);
			  	// dprintf("me_avg %f \n", me_avg);
			  	// dprintf("date.day %d date.month %d date.year %d\n",date.day,date.month,date.year);
				// dprintf("prob %f\n",prob);
				// dprintf("ieff_avg %f = ieff_patch %f / fpc_patch %f \n",ieff_avg,ieff_patch,fpc_patch);
				// // dprintf("burnedf %f ieff %f prob %f \n",burnedf,ieff,prob);
			// }
		if (ieff > prob)
		{
			nlig = 1;
		}
		else
		{
			nlig = 0;
		}
	}
	else
	{
		nlig = 0;
	}
	
	// revised by weichao---------------------------------------------------------------------------------------------
	//! this is for King Fire in 2014
	// int human_ignition_year;
	// human_ignition_year = 2014 - first_sim_year;
	// if(date.year-nyear_spinup == human_ignition_year && date.month >= 0){
		// nlig = 1;
	// }
		
	//end of revision by weichao	

	/////////////////////////////////////////////////////////////////////////////////////////
	//// FIRE SPREAD (mean fire area)
	//
	//	!---------------------------
	//	!part 2.2.4, mean fire area (rate of spread)
	//
	//	!characterize fuel that is carrying the fire in terms of mass, bulk density, surface area to volume ratio, and moisture content

	//	!---
	//	!fuel mass
	double lfuelw[npft][4];	  //! live fuel load in 1- 10- 100- and 1000- hour fuel classes that is wood (g DM m-2)
	double dfuelw[npft][4];	  //! dead fuel load in 1- 10- 100- and 1000- hour fuel classes that is wood (g DM m-2)
	double lfuelg[npft][4];	  //! live fuel load in 1- 10- 100- and 1000- hour fuel classes that is grass (g DM m-2)
	double dfuelg[npft][4];	  //! dead fuel load in 1- 10- 100- and 1000- hour fuel classes that is grass (g DM m-2)
	bool fuelpft[npft];		  //! true if there is nonzero fuel for this PFT
	double pftlivefuel[npft]; //! mass of live herbaceous fuel per pft summed over 1- 10- and 100-h fuel classes (kg m-2)
	double pftdeadfuel[npft]; //! mass of dead fuel per pft summed over 1- 10- and 100-h fuel classes (kg m-2)
	double rho_pft[npft];	  //! bulk density of dead fuel per pft mass weighted average over 1- 10- and 100-h fuel classes (kg m-3) //NOTE KG!
	double Mx[nspec];		  //! trace gas emissions (g x m-2)
	// initialize
	for (i = 0; i < npft; i++)
	{
		for (j = 0; j < 4; j++)
		{
			lfuelw[i][j] = 0.;
			dfuelw[i][j] = 0.;
			lfuelg[i][j] = 0.;
			dfuelg[i][j] = 0.;
		}
		pftlivefuel[i] = 0.;
		pftdeadfuel[i] = 0.;
		rho_pft[i] = 0.;
	}
	double totlfuelw = 0.;
	double totdfuelw = 0.;
	double totlfuelg = 0.;
	double totdfuelg = 0.;
	double livemass = 0.;
	double deadmass = 0.;
	double rho_livegrass;
	double slopefact;

	double tfire; //! fire duration (min)

	double Abfrac; //! fractional area burned on the gridcell (fraction)
	double DT;	   //! length of the major axis of the fire (total distance traveled) (m)

	int numfires_nat;			 //! number of fires started on the grid cell on current day, patch for LPJ-GUESS
	int numfires;				 //! number of fires started on the grid cell on current day
	numfires = numfires_nat = 0; // initialize

	double LB;			  //! length-to-breadth ratio of burn ellipse
	double LBgrass;		  //! length-to-breadth ratio of burn ellipse
	double LBtree;		  //! length-to-breadth ratio of burn ellipse
	double ROSfsurface;	  //! overall forward rate of spread of fire (m min-1)
	double ROSfsurface_g; //! forward rate of spread of fire in herbaceous fuel (m min-1)
	double ROSfsurface_w; //! forward rate of spread of fire in woody fuels (m min-1)
	double ROSfcrown;	  //! forward rate of spread of fire in crown (m min-1)
	double ROSbsurface;	  //! backward rate of spread of fire (m min-1)

	double Ab = 0.;		//! area burned (ha d-1)
	double rabarf = 0.; // raw abarf before limited by patch size
	double abarf = 0.;
	double BBpft = 0.; //! daily total biomass burned from each PFT, sum of live and dead consumed (kg dry matter m-2)
	double aMx[npft];  //! annual trace gas emissions (g m-2; per species) CHANGE this needs to be GLOBAL variable or passed to one
	if (date.day == 0)
	{ // initialize
		for (i = 0; i < npft; i++)
		{
			aMx[i] = 0.0;
		}
	}
	double gscale;

	// Initialize
	//!------------------
	//! annual stats

	if (date.day == 0)
	{

		patch.anumfires = 0; // int
		patch.burndays = 0;	 // int

		patch.precsum = 0.;
		patch.unburneda = 0.;
		patch.sumfdi = 0.;

		//  peopfire_account = 365

	} // end initalization date.day = 0

	// monthly stats
	if (date.dayofmonth == 0)
	{ // initialize
		patch.mnumfires[date.month] = 0.;
		patch.mfdi[date.month] = 0.;
	}

	i = 0;
	pftlist.firstobj();
	while (pftlist.isobj)
	{
		Pft &pft = pftlist.getobj();
		if (pft.lifeform == TREE)
		{
			// if(date.islastday && date.islastmonth){
			// dprintf("pft.lifeform = %d NOLIFEFORM, TREE, GRASS, CROP \n", pft.lifeform);
			// }
			for (j = 0; j < 4; j++)
			{
				lfuelw[i][j] += livefuel[i][j];
				dfuelw[i][j] += deadfuel[i][j];
			}
		}
		else
		{
			for (j = 0; j < 4; j++)
			{
				// lfuelg[i][j] += livefuel[i][j];
				dfuelg[i][j] += deadfuel[i][j];
			}
		}
		pftlist.nextobj(); // ... on to next PFT
		i++;
	}

	for (i = 0; i < npft; i++)
	{
		for (j = 0; j < 4; j++)
		{
			totlfuelw += lfuelw[i][j];
			totdfuelw += dfuelw[i][j];
			//		totlfuelg += lfuelg[i][j];
			totdfuelg += dfuelg[i][j];
		}
	}

	for (i = 0; i < npft; i++)
	{
		for (j = 0; j < 3; j++)
		{
			// pftlivefuel !mass of live herbaceous fuel per pft summed over 1- 10- and 100-h fuel classes (kg m-2)
			pftlivefuel[i] += (livefuel[i][j] / 1000.); // Note only 1- 10- and 100- hour fuel classes / 1000. to convert to kg
			// pftdeadfuel  !mass of dead fuel per pft summed over 1- 10- and 100-h fuel classes (kg m-2)
			pftdeadfuel[i] += (deadfuel[i][j] / 1000.); // Note only 1- 10- and 100- hour fuel classes / 1000. to convert to kg
		}
	}

	for (i = 0; i < npft; i++)
	{
		if (pftdeadfuel[i] > 0)
		{
			fuelpft[i] = true;
		}
		else
		{
			fuelpft[i] = false;
		}
	}

	i = 0;
	pftlist.firstobj();
	while (pftlist.isobj)
	{
		Pft &pft = pftlist.getobj();
		if (fuelpft[i] == true)
		{
			deadmass += pftdeadfuel[i];
		}
		if (pft.lifeform != TREE)
		{
			livemass += pftlivefuel[i];
		}
		pftlist.nextobj(); // ... on to next PFT
		i++;
	}

	Soil &soil = patch.soil; // Retrieve Soil objects for patch

	//------------------------------------------------------------------------------------------------------------------
	// Extinguish fires or ignition if too much snow or rain
	// Update FDI (only if not extinguished)

	// No fire on days with snow on the ground
	if (soil.snowpack > 0)
	{						//! no fire on days with snow on the ground
		patch.cumfires = 0; //! extinguish all smouldering or burning fires
		// dprintf("Snowpack > 0, fires extinguished\n");
		return;
	}

	// Extinguish smoldering or burning fires if too much rain accumulation
	// if (stand.climate.prec == 0) {
	if (gridcell.climate.prec == 0)
	{ // modified by weichao
		patch.precsum = 0.;
	}
	else
	{
		// patch.precsum = patch.precsum + stand.climate.prec;
		patch.precsum = patch.precsum + gridcell.climate.prec; // modified by weichao
	}

	if (((patch.precsum >= 10) && (grasscover <= 0.6)) || ((patch.precsum >= 3) && (grasscover > 0.6)))
	{ //! extinguish all currently burning or smoldering fires
		patch.cumfires = 0;
	}

	//// No fire on days with too much rain
	// if (stand.climate.prec > 0) {
	if (gridcell.climate.prec > 0)
	{ // modified by weichao
		patch.cumfires = 0;
		// dprintf("High precipitation, fires extinguished\n");
		return; // HIGH PREC TEST
	}

	// revised by weichao
	// No fire on days with too little fuel
	double totfuel = (totlfuelw + wlivegrass + totdfuelg);
	// double totfuel = (totlfuelw + wlivegrass + totdfuelg + cpool_surf);
	// end of revision by weichao

	//! added by weichao 2023-1-8
	// if ((Abfrac < 0.01))
	// {						// || (totvcover < 0.5)){ CHANGE!!!! NEW JULY
		// patch.cumfires = 0; //! extinguish all smouldering or burning fires
							// //	dprintf("NO FUEL totfuel %f totfuelw %f wlivegrass %f totdfuelg %f cpool_surf %f OR totvcover %f\n",
							// //			totfuel,totlfuelw,wlivegrass,totdfuelg,cpool_surf,totvcover);
		// return;
	// }
	//

	if ((totfuel < 1000.))
	{						// || (totvcover < 0.5)){ CHANGE!!!! NEW JULY
		patch.cumfires = 0; //! extinguish all smouldering or burning fires
							//	dprintf("NO FUEL totfuel %f totfuelw %f wlivegrass %f totdfuelg %f cpool_surf %f OR totvcover %f\n",
							//			totfuel,totlfuelw,wlivegrass,totdfuelg,cpool_surf,totvcover);
		return;
	}

	// Update monthly sum FDI - so only if there's an initially successful ignition (nlig = 1)
	patch.mfdi[date.month] += FDI / (date.ndaymonth[date.month]); // Average FDI over the course of a month

	// dprintf("patch.mfdi %f date.day %d date.month %d date.year %d\n"
	// ,patch.mfdi[date.month],date.day,date.month,date.year);

	// Update initially successful lightning-caused ignition events
	patch.mnlig[date.month] += nlig;
	// if ((date.year > startHisClim) && (nlig>0)){
	//	dprintf("lightning-caused ignition events nlig = %f patch.mnlig %f date.year %d\n",nlig, patch.mnlig[date.month],date.year);
	// }

	////	!mean bulk density for each PFT across fuel classes
	//!---
	//! bulk density of standing grass biomass (function of average GDD)
	//! ranges from about 12 at tundra GDDs to 1 for tropical GDDs

	// double gdd20 = stand.climate.magdd20; // accumulated growing degree days (20-year running mean) @KE added
	// modified by weichao
	double gdd20 = gridcell.climate.magdd20; // accumulated growing degree days (20-year running mean) @KE added

	rho_livegrass = 2.e4 / (gdd20 + 1000.) - 1.;
	if (date.year == 0)
	{
		rho_livegrass = 0.0; // because this is used before gdd is calculated and we don't need fire the first year
	}
	// dprintf("gdd20 %f rho_livegrass %f \n", gdd20, rho_livegrass);

	i = 0;
	pftlist.firstobj();
	while (pftlist.isobj)
	{
		Pft &pft = pftlist.getobj();
		if (pft.lifeform == GRASS)
		{
			rhobPFT[i] = rho_livegrass; //! because the bulk density of grass litter should not be less than the bulk density of the live grass
		}
		if (fuelpft[i] == true)
		{
			rho_pft[i] = rhobPFT[i] * (deadfuel[i][0] + 1.2 * deadfuel[i][1] + 1.4 * deadfuel[i][2]) / (deadfuel[i][0] + deadfuel[i][1] + deadfuel[i][2]);
		}
		pftlist.nextobj(); // ... on to next PFT
		i++;
	}

	//!------------------------------------------------------------------------------------------------------------------
	//! Rate of spread calculations

	//!---
	//! surface ROS in grass litter

	relmoist = omega_nl / me_nl;
	// added by weichao to limit relmoist range
	relmoist = min(max(relmoist, 0.0), 1.0);
	// Uforward !mean wind speed (m min-1)
	// Ustar !day mean wind speed (m s-1)
	//  Ustar = stand.climate.wind;
	Ustar = gridcell.climate.wind; // modified by weichao
	// if(date.year-nyear_spinup == 30 && date.month == 8){
		// dprintf("Windspeed = %f m/s \n",Ustar);
	// }

	Ustar = max(Ustar, 0.);
	Uforward = 60. * Ustar;

	if (relmoist < 1)
	{

		// gscale = -0.0848 * min(rho_livegrass,12) + 1.0848;    //!scale factor for bulk density of grass fuel, reduces rate of spread in high bulk density grasses (e.g., tundra)

		// modified by weichao (int to double)
		gscale = -0.0848 * min(rho_livegrass, 12.0) + 1.0848; //! scale factor for bulk density of grass fuel, reduces rate of spread in high bulk density grasses (e.g., tundra)

		ROSfsurface_g = (0.165 + 0.534 * Uforward / 60.) * exp(-0.108 * relmoist * 100.) * gscale * 60.; //! from Mell_etal2008, eqn. 2
	}
	else
	{

		ROSfsurface_g = 0.;
	}


	//!---
	//! surface ROS in woody litter

	wn = livemass + deadmass; // kg m-2
	// dprintf("wn %f = livemass %f + deadmass %f\n",wn,livemass,deadmass);

	double sum_rho_pftdf = 0.;
	for (i = 0; i < npft; i++)
	{
		sum_rho_pftdf += rho_pft[i] * pftdeadfuel[i];
	}

	rho_b = (rho_livegrass * livemass + sum_rho_pftdf) / wn; // Note only 1- 10- 100- and 1000- hour fuel classes

	if (wn == 0)
	{
		rho_b = 0.0; // to avoid division by zero
	}
	// dprintf("rho_b %f rho_livegrass %f livemass %f sum_rho_pftdf %f wn %f\n",rho_b,rho_livegrass,livemass,sum_rho_pftdf,wn);

	sigma = 5.; // LMFire development from SPITFIRE
	// sigma = sigma_i[0];     //!typical value for the fire-carrying fuelbed (cf. Scott & Burgan, 2005)

	relmoist = patch.omega_o / me_avg;
	// dprintf("relmoist %f = patch.omega_o %f / me_avg %f date.year %d date.day %d\n",relmoist,patch.omega_o,me_avg,date.year,date.day);
    omega_o = patch.omega_o;
	if (relmoist < 1 && rho_b != 0.0)
	{					 //! FDI not zero for this landscape component //@KE added rho=!0 conditional so doesn't result in inf
		wn = wn * 1000.; // wn needs to be in g m-2 for calcROS()
		calcROS();		 // wn, rho_b, sigma, patch.omega_o, relmoist, Uforward, rateofspread
		ROSfsurface_w = rateofspread;
	}
	else
	{
		ROSfsurface_w = 0.;
	}
    if(std::isnan(ROSfsurface_w)){ROSfsurface_w = 0.0;}

	// test ROS by weichao
	if (std::isnan(ROSfsurface_w))
	{
		dprintf("Attention: rateofspread problems------------------------\n");
	}
	// end of test by weichao

	////!---
	////!surface ROS in woody litter using grass litter equations - 5/8/19 TEST
	//
	////using grass surface area to volume ratio
	// sigma = sigma_i[0];     //!typical value for the fire-carrying fuelbed (cf. Scott & Burgan, 2005)  //CHANGE these assigned here for grasses, not used, only to be reassigned below for woody litter
	////using grass litter moisture and moisture of extinction (but woody me is higher!)
	////relmoist = omega_nl / me_nl;
	////using grass litter moisture but woody me
	// relmoist = omega_nl / me_avg;
	//
	////Uforward !mean wind speed (m min-1)
	////Ustar !day mean wind speed (m s-1)
	// Ustar = stand.climate.wind;
	// Ustar = max(Ustar, 0.);
	// Uforward = 60. * Ustar;
	//
	// if (relmoist < 1 && rho_b != 0.0) {       //!FDI not zero for this landscape component //@KE added rho=!0 conditional so doesn't result in inf
	//
	//   //replaced rho_livegrass with rho_b from above (fuel bulk density)
	//   gscale = -0.0848 * min(rho_b,12) + 1.0848;    //!scale factor for bulk density of grass fuel, reduces rate of spread in high bulk density grasses (e.g., tundra)
	//
	//   ROSfsurface_w = (0.165 + 0.534 * Uforward / 60.) * exp(-0.108 * relmoist * 100.) * gscale * 60.;     //! from Mell_etal2008, eqn. 2
	// }
	// else {
	//
	//   ROSfsurface_w = 0.;
	// }

	/////////////////////////////////////////////////////////////////////////////////////////
	// Crown fire initiation and spread

	// part 2.2.4 - part 2, added by KE 2019

	// ARVE-Reserach/LPJ-LMfire version 1.3 on GitHub
	//!---
	//! crown ROS

	ROSfcrown = 0.0;
	double lfuelw0; // mass of 1-hr fuel in living biomass
	lfuelw0 = 0.;
	double canopywater; //! integrated mean wscal
	canopywater = 0.;
	// wscal          !water scalar (supply/demand <= 1) per pft, on this day !mean daily water scalar (among leaf-on days) (0-1 scale)

	// if (treecover >= 0.6){
	//
	//	// *** Loop through PFTs ***
	//	pftlist.firstobj();
	//	while (pftlist.isobj) {
	//
	//		Pft& pft=pftlist.getobj();
	//		Vegetation& vegetation=patch.vegetation;
	//		Patchpft& patchpft=patch.pft[pft.id];
	//
	//		if (pft.lifeform==TREE) {
	//
	//			lfuelw0 += livefuel[pft.id][0]; //!mass of 1-hr fuel in living biomass  !FLAG maximum leaf dry mass set to 8 kg m-2!
	//		}
	//
	//	// Loop through cohorts
	//	vegetation.firstobj();
	//	while (vegetation.isobj) {
	//		Individual& indiv=vegetation.getobj();
	//
	//			if ((indiv.pft.lifeform==TREE) && (indiv.pft.id == pft.id)) {
	//
	//				canopywater +=  (indiv.wscal * indiv.fpc) / treecover;
	//				//canopywater = sum(wscal * fpc_grid, mask=pft%tree) / treecover;
	//
	//			}
	//
	//			vegetation.nextobj();
	//		} //end vegetation loop
	//
	//		// ... on to next PFT
	//		pftlist.nextobj();
	//	} // end pft loop
	//
	//	if (date.year > 1000) {
	//		dprintf("canopywater %f \n",canopywater);
	//	}
	//
	//     if (canopywater < 0.8){   //0.01!calculate rate of spread in crown
	//
	//       wn = lfuelw0;
	//
	//   	if (date.year > 1000) {
	//         dprintf("low canopywater %f \n",canopywater);
	//         dprintf("leafmass %f \n",wn);
	//   	}
	//
	//       wn = min(8000.,wn);
	//
	//       rho_b = 0.1;
	//       sigma = sigma_i[0];
	//       relmoist = 0.99;  //!at moisture content just below extinction fire can spread
	//
	//       //CHANGE LMfire v 1.3 sets here patch.omega_o = 0.3, but we want this to track so hold temporarily and reassign
	//       double temp_omega = patch.omega_o;
	//       patch.omega_o = 0.3;
	//
	//       calcROS(); // wn, rho_b, sigma, omego_o, relmoist, Uforward, rateofspread
	//       //calcROS(orgf*wn,rho_b,sigma,0.3,relmoist,Uforward,ROSfcrown);
	//
	//       ROSfcrown = rateofspread;
	//
	//       patch.omega_o = temp_omega;
	//
	//     }
	//
	// } //end treecover > 0.6 conditional

	// Active crown fire criteria - Van Wagner 1977, 1993

	// crown ROS - Rothermel 1991

	// crown ROS - Cruz 2006

	//!---
	//! calculate weighted average surface rate of spread on tree and grass fractions
    if((treecover + grasscover) > 0.0){
		ROSfsurface = (ROSfsurface_w * treecover + ROSfsurface_g * grasscover) / (treecover + grasscover);
	}
	

	ROSfsurface = max(ROSfsurface, 0.0); // added by weichao due to nan value
    	// added by weichao
	if(std::isnan(ROSfsurface)){
		ROSfsurface = 0.0;
	}
	// end of addition by weichao

		// *** Loop through PFTs ***
	pftlist.firstobj();
	while (pftlist.isobj)
	{

		Pft &pft = pftlist.getobj();
		Vegetation &vegetation = patch.vegetation;
		Patchpft &patchpft = patch.pft[pft.id];

		vegetation.firstobj();
		while (vegetation.isobj)
		{
			Individual &indiv = vegetation.getobj();

			if ((indiv.pft.id == pft.id))
			{
                // added by weichao
				if(date.day == 0)
					indiv.ROS = 0.0;
                indiv.ROS += ROSfsurface;

				// if(date.year-nyear_spinup == 20){
					// dprintf("pft.id = %d ROS = %f\n",pft.id, indiv.ROS);
				// }

			} // end if TREE
				// ... on to next individual
			vegetation.nextobj();
		} // end cohort loop

			// ... on to next PFT
		pftlist.nextobj();
	} // end pft loop
	// if ((date.year == 600) && (nlig > 0)){ 
	// dprintf("\nROSfsurface_w %.2f Fx = wn %.2f rho_b %.2f sigma %.2f patch.omega_o %.2f relmoist %.2f Uforward %.2f \n",
	// ROSfsurface_w, wn, rho_b, sigma, patch.omega_o, relmoist, Uforward);
	// dprintf("ROSfsurface_g %.2f = (0.165 + 0.534 * Uforward %.2f / 60.) * exp(-0.108 * relmoist %.2f * 100.) * gscale %.2f * 60.   omega_nl %f  me_nl %f\n",
	// ROSfsurface_g, Uforward, omega_nl / me_nl, gscale, omega_nl, me_nl);
	// dprintf("ROSfsurface %.2f = ROSfsurface_w %.2f treecover %.2f ROSfsurface_g %.2f grasscover %.2f \n",
	// ROSfsurface,ROSfsurface_w,treecover,ROSfsurface_g,grasscover);
	// //	dprintf("ROSfcrown %f Fx = wn %f rho_b %f sigma %f patch.omega_o %f relmoist %f Uforward %f \n",
	// //			ROSfcrown, wn, rho_b, sigma, 0.3, relmoist, Uforward);
	// }

	//! choose max from above calculations
	// ARVE-Reserach/LPJ-LMfire version 1.3 on GitHub

	// if (ROSfcrown > ROSfsurface) {        //!set definition of logical "crownfire" here.......
	////!    crownfire = .true.
	//    ROSfsurface = ROSfcrown;
	//
	//    if ((date.year > 990) && (nlig > 0)){
	//    dprintf("CROWN FIRE!\n ROSfcrown %f\n",ROSfcrown);
	//    }
	//}

	//!---------------------------
	//! backward rate of spread - decreases with stronger wind

	ROSbsurface = ROSfsurface * exp(-0.012 * Uforward);

	// if (ROSfsurface > 0.0){
	//	dprintf("ROSfsurface = %f ROSbsurface = %f \n",ROSfsurface,ROSbsurface);
	// }
	// if (treecover != 0.0 && ROSfsurface > 0.0){
	//	dprintf("ROSfsurface = %f ROSbsurface = %f \n",ROSfsurface,ROSbsurface);
	// }
	//!---
	//! length-to-breadth ratio of burn ellipse

	if (Uforward >= 16.67)
	{ //! windspeed exceeds 1 km/h

		Ukmh = 0.06 * Uforward; //! convert to km h-1 for the following equations

		LBtree = 1. + 8.729 * pow((1 - exp(-0.03 * Ukmh)), 2.155); //! eqn. 12

		LBgrass = 1.1 + pow(Ukmh, 0.0464); //! eqn. 13 as in the f77 code from Allan

		if (totvcover > 0)
		{
			LB = (LBtree * treecover + LBgrass * grasscover); //! weighted average
		}
		else
		{
			LB = LBgrass;
		}
	}

	else
	{
		LB = 1;
	}

	LB = min(LB, 8.); //! limit LB to a maximum of 8 as in f77 code from Allan

	//!---
	//! fire duration

	tfire = 241. / (1. + 240. * exp(-11.06 * FDI)); //! eqn. 14 (minutes)

	//!---
	//! total distance traveled

	DT = tfire * (ROSfsurface + ROSbsurface);

	//!---
	//! mean fire area

	// dprintf("\navg_cont_area %f\n",avg_cont_area); // before avg_cont_area = patch area of 0.1 ha //CHANGE - why 10 when 1 ha? Also does not match my patch size!
	avg_cont_area = max(avg_cont_area, 10.); //! assumption is that the smallest possible size of a kernel is 1 ha: nothing will be fractionated to a size less than 1 ha
	// dprintf("max(avg_cont_area, 10) %f\n",avg_cont_area); // after avg_cont_area = 10 ha

	// Retrieve coordinate of next grid cell from linked list
	// Coord& c = gridlist.getobj();

	// if (gridlist.getobj().slope >= 1.72) {   //!0.03 for radians
	// //!   slopefact = 1. / (100. * input%slope - 2.)                !slope > 0.03, for slope coming in as radians
	// slopefact = (1. / ((((5. / 9.) * PI) * gridlist.getobj().slope) - 2.));          //! this one for slope coming in in degrees
	// }
	// else {
	// slopefact = 1.;
	// }
	// slopefact = -0.54;  // modified by weichao slope = 0 degree
	slopefact = 1; // modified by weichao slope = 1 degree

	rabarf = (PI / (4. * LB) * pow(DT, 2.)) * 0.0001 * slopefact; //! average size of an individual fire (eqn. 11) (ha)
	if(std::isnan(rabarf)){dprintf("warning--------------rabarf = %f\n",rabarf);}
	// if(nlig>0){
	//	dprintf("rabarf %f = PI %f / (4*LB %f) * pow(DT %f,2) *0.0001 *slopefact %f\n",rabarf,PI,LB,DT,slopefact);
	// }

	abarf = min(rabarf, avg_cont_area); //! the size of an individual fire is not allowed to be greater than the average contiguous patch size
	
	
	// if(nlig>0){
	//	dprintf("abarf = min(abarf, avg_cont_area) = %f\n",abarf);
	// }

	// THIS IS WHERE YOU WOULD ADD HUMAN-CAUSED FIRES
	// nhig                    !number of human-caused ignition events (per gridcell d-1)
	// nhig = nhig * riskfact       ! reduce number of fires caused when FDI gets above 0.25 (conservative approach by people who don't want to risk overburning their targets on days with higher fire risk)

	numfires_nat = nlig; //! lightning fires started on this day

	// numfires_hum = nhig		//!human fires started on this day

	numfires = numfires_nat; // CHANGE HERE when add human fires
	// numfires = numfires_nat + numfires_hum
	//  if (patch.cumfires != 0){
	//  dprintf("CUMFIRE %d date.year %d\n",patch.cumfires,date.year);
	// }
	patch.cumfires += numfires - round(patch.burnedf * double(patch.cumfires + numfires)); //! allow naturally-caused fires to carry over from one day to the next
    if(patch.totburn > area_ha){dprintf("warning--------------burn too much area (patch.totburn) = %f\n",patch.totburn);}

	patch.unburneda = area_ha - patch.totburn;
	patch.unburneda = max(0., patch.unburneda);

    if(std::isnan(patch.cumfires)){dprintf("warning--------------patch.cumfires = %f\n",patch.cumfires);}
	if(std::isnan(abarf)){dprintf("warning--------------abarf = %f\n",abarf);}
	if(std::isnan(Ab)){dprintf("warning--------------Ab = %f\n",Ab);}

	Ab = max(0., min(patch.unburneda, patch.cumfires * abarf)); //! ha
	if(std::isnan(Ab)){dprintf("warning--------------Ab = %f\n",Ab);}
	// if(nlig>0){
	// dprintf("Ab %f = min(unburneda %f, cumfires %d * abarf %f)\n",Ab,patch.unburneda,patch.cumfires,abarf);
	// dprintf(" numfires %d \n",numfires);
	// }

	Abfrac = Ab / area_ha; // ha

	//! added by weichao 2023-1-8
	if ((Abfrac < 0.01))
	{						// || (totvcover < 0.5)){ CHANGE!!!! NEW JULY
		patch.cumfires = 0; //! extinguish all smouldering or burning fires
							//	dprintf("NO FUEL totfuel %f totfuelw %f wlivegrass %f totdfuelg %f cpool_surf %f OR totvcover %f\n",
							//			totfuel,totlfuelw,wlivegrass,totdfuelg,cpool_surf,totvcover);
		return;
	}
	

	if (Abfrac > 1 || Abfrac < 0 || patch.unburneda < 0)
	{
		dprintf("ERROR: Abfrac problem\n");
		// dprintf("FDI %f, abarf %f, Abfrac %f, Ab %f, unburneda %f, numfires %d, totburn %f, area_ha %f\n", FDI,abarf,Abfrac,Ab,unburneda,numfires,totburn,area_ha);
		exit(0);
	}

	//! total area burned (ha)

	//! Ab = Abfrac * area_ha        !eqn. 2

	if (Ab == 0)
	{
		//	if ((date.year > startHisClim) && (nlig>0)){
		//		dprintf("Area burned = zero, nlig %d patch.cumfires %d lmfire() ended, date.year = %d\n",nlig,patch.cumfires,date.year);
		//	}
		return;
	}
	// if(nlig>0){
	// dprintf("Ab %f = min(unburneda %f, cumfires %d * abarf %f)\n",Ab,patch.unburneda,patch.cumfires,abarf);
	// dprintf(" numfires %d \n",numfires);
	// }

	/////////////////////////////////////////////////////////////////////////////////////////
	//// FIRE EFFECTS
	//
	/////////////////////////////////////////////////////////////////////////////////////////
	//// Calculates fractional combustion of dead fuel & fire damage to living plants
	//

	double acflux_fire = 0.;	// !carbon flux from biomass burning (g C m-2 d-1)
	double acflux_fireveg = 0.; // !carbon flux from biomass burning only live vegetation (g C m-2 d-1) @KE added
	double BBdead[npft][5];		//! biomass burned from dead fuel by fuel type and PFT (g m-2)
							// 1- fast, 1- slow, 10- 100- and 1000- hour fuel classes
	for (i = 0; i < npft; i++)
	{ // initialize
		for (j = 0; j < 5; j++)
		{
			BBdead[i][j] = 0.;
		}
	}
	double BBlive[npft][3]; //! biomass burned from live fuel by fuel type and PFT (g m-2)
							// 1- 10- and 100- hour fuel classes
	for (i = 0; i < npft; i++)
	{ // initialize
		for (j = 0; j < 3; j++)
		{
			BBlive[i][j] = 0.;
		}
	}

	//! biomass consumption or mortality through wildfire - initialized at start of year
	double annBBdead[npft][5]; //! annual total biomass burned from dead fuel by fuel type and PFT (g m-2)
	double annBBlive[npft][3]; //! annual biomass burned from live fuel by fuel type and PFT (g m-2)
	if (date.day == 0)
	{ // initialize
		for (i = 0; i < npft; i++)
		{
			for (j = 0; j < 5; j++)
			{
				annBBdead[i][j] = 0.0;
			}
			for (j = 0; j < 3; j++)
			{
				annBBlive[i][j] = 0.0;
			}
		}
	}
	double BBtot; //! total biomass burned (g C m-2)

	//!----------------------------------------------
	//! part 2.2.5, fractional combustion of dead fuel

	//!---
	//! fraction of live grass consumed in surface fire
	double CFlg; //! consumed fraction of live grass

	rm = omega_lg / me_lf;
	rm = min(rm, 1.); // added by weichao

	// if(date.day > 240 && date.day < 300 && date.year > 600){
	// dprintf("rm %f  omega_lg %f  me_lf  %f\n",rm,omega_lg,me_lf);
	// }

	if (rm <= 0.18)
	{
		CFlg = 1.;
	}
	else if (rm > 0.73)
	{
		CFlg = 2.45 - 2.45 * rm;
	}
	else
	{
		CFlg = 1.10 - 0.62 * rm; //! coefficients from Allan's Spitfire code
	}

	//@KE do not let CFlg be less than zero or greater than one
	CFlg = min(max(CFlg, 0.), 1.);
	// dprintf("CFlg %f rm %f omega_lg %f me_lf %f\n",CFlg,rm,omega_lg,me_lf);
	
	//!---
	//! fractional consumption of dead fuel per fuel class 1- 10- 100- and 1000- hour fuel classes
	double CF[4]; //! fractional consumption of dead fuel per fuel class 1- 10- 100- and 1000- hour fuel classes
	double FC[4]; //! amount of dead fuel consumed, per fuel class (g m-2)

	//!---
	//! fraction of 1-h fuel consumed in surface fire

	rm = omega[0] / me_fc[0];
	rm = min(rm, 1.); // added by weichao

	// if(date.day > 240 && date.day < 300 && date.year > 600){
	// dprintf("rm------0 %f  omega %f  me_fc  %f\n",rm,omega[0],me_fc[0]);
	// }

	if (rm <= 0.18)
	{
		CF[0] = 1.;
	}
	else if (rm > 0.73)
	{
		CF[0] = 2.45 - 2.45 * rm;
	}
	else
	{
		CF[0] = 1.10 - 0.62 * rm; //! coefficients from Allan's Spitfire code
	}

	//!---
	//! fraction of 10-h fuel consumed in surface fire

	rm = omega[1] / me_fc[1];
	rm = min(rm, 1.); // added by weichao

	// if(date.day > 240 && date.day < 300 && date.year > 600){
	// dprintf("rm------1 %f  omega %f  me_fc  %f\n",rm,omega[1],me_fc[1]);
	// }

	if (rm <= 0.12)
	{
		CF[1] = 1.;
	}
	else if (rm > 0.51)
	{
		CF[1] = 1.47 - 1.47 * rm;
	}
	else
	{
		CF[1] = 1.09 - 0.72 * rm;
	}

	//!---
	//! fraction of 100-h fuel consumed in surface fire

	rm = omega[2] / me_fc[2];
	rm = min(rm, 1.); // added by weichao

	// if(date.day > 270 && date.day < 300 && date.year > 600){
	// dprintf("rm------2 %f  omega %f  me_fc  %f\n",rm,omega[2],me_fc[2]);
	// }

	if (rm <= 0.38)
	{
		CF[2] = 0.98 - 0.85 * rm;
	}
	else
	{
		CF[2] = 1.06 - 1.06 * rm;
	}

	//!---
	//! fraction of 1000-h fuel consumed in surface fire

	rm = omega[3] / me_fc[3];
	if(std::isnan(rm)){dprintf("Warning------------------------------rm = %f\n",rm);}
	rm = min(rm, 1.); // added by weichao
	// if(date.day > 240 && date.day < 300 && date.year > 600){
	// dprintf("rm------3 %f  omega %f  me_fc  %f\n",rm,omega[3],me_fc[3]);
	// }

	CF[3] = -0.8 * rm + 0.8;

	//!---
	//! correct for spurious values of consumed fraction
	for (i = 0; i < 4; i++)
	{
		// dprintf("BEFORE CF[i] %f\n",CF[i]);
		if(std::isnan(CF[i])){dprintf("Warning------------------------------FC = %f\n",CF[i]);}
		CF[i] = min(max(CF[i], 0.), 1.);
		// dprintf("AFTER CF[i] %f\n",CF[i]);
	}


	//!---
	//! total fuel consumed (g m-2), mineral fraction subtracted
	for (i = 0; i < 4; i++)
	{
		FC[i] = CF[i] * woi[i] * (1. - ST);
		if(std::isnan(FC[i])){dprintf("Warning------------------------------FC = %f\n",FC[i]);}
		// if (date.day == 0)
		// {
			// printf("FC[i] %f = CF[i] %f * woi[i] %f * (1 - ST %f)\n",FC[i],CF[i],woi[i],ST);
		// }
	}
	// for (i=2; i<4; i++){
	//	if (CF[i] != 0.0){
	//	printf("omega[i] %f / me_fc[i] %f\n",omega[i],me_fc[i]);
	//	printf("FC[i] %f = CF[i] %f * woi[i] %f * (1 - ST %f)\n",FC[i],CF[i],woi[i],ST);
	//	}
	// }

	//!---
	//! surface fire intensity (kW m-1)

	// Isurface = h * (FC[0] + FC[1] + FC[2]) * ROSfsurface * min2sec;  //!eqn. 15
	Isurface = h * (FC[0] + FC[1] + FC[2]) * ROSfsurface * 1.0 / 60.0; //! eqn. 15

	// *** Loop through PFTs ***
	pftlist.firstobj();
	while (pftlist.isobj)
	{

		Pft &pft = pftlist.getobj();
		Vegetation &vegetation = patch.vegetation;
		Patchpft &patchpft = patch.pft[pft.id];

		vegetation.firstobj();
		while (vegetation.isobj)
		{
			Individual &indiv = vegetation.getobj();

			if ((indiv.pft.id == pft.id))
			{
                // added by weichao
				if(date.day == 0)
					indiv.SH = 0.0;
				
                indiv.SH += 0.148 * pow(Isurface, 0.667);

			} // end if TREE
				// ... on to next individual
			vegetation.nextobj();
		} // end cohort loop

			// ... on to next PFT
		pftlist.nextobj();
	} // end pft loop


	// if (date.year - nyear_spinup == 100 && date.day == 240)
	// {
		// dprintf("Isurface %f  h %f FC[0] %f FC(1) %f FC(2) %f ROSfsurface %f date.day %d \n", Isurface, h, FC[0], FC[1], FC[2], ROSfsurface, date.day);
	// }
	// dprintf("Isurface %f  date.year %f \n",Isurface, date.year);

	// if (Isurface < 100) { //50 @KE TESTING!!!    //!ignitions are extinguished and there is no fire on this day 
	
	// added by weichao
	if(std::isnan(Isurface)){
		dprintf("Warning:Isurface = %f\n",Isurface);
		// Isurface = 0.0;
	}
	// end of addition by weichao
	if (Isurface <= 100.0)
	// if (Isurface <= 100.0 && date.year-nyear_spinup < human_ignition_year)
	{ // 50 @KE TESTING!!!    //!ignitions are extinguished and there is no fire on this day

		Ab = 0.;
		Abfrac = 0.;
		//	if (date.year > startHisClim){ 
		//		dprintf("Isurface < 100, Area burned set to zero, lmfire() ended, date.year = %d\n",date.year);
		//	}
		return; //! leave the fire subroutine
	}
	// else if(date.year-nyear_spinup == human_ignition_year){
		// Ab = 0.5;
		// Abfrac = 0.5;
	// }
	// else {
		// dprintf("Warning:Isurface = %f\n",Isurface);
	// }

	// dprintf("Isurface %.2f = h %.2f * (FC[0] %.2f + FC[1] %.2f + FC[2] %.2f) * ROSfsurface %.2f / 60"
	// "\n",Isurface,h,FC[0],FC[1],FC[2],ROSfsurface,min2sec);

	// Start of new code @KE crown fire modeling
	if (firemode == LMFIRE_CF)
	{
		//////////////////////////////////////////////////////////////////////////////////////////////
		// Calculate canopy conditions (canopy bulk density and canopy foliar moisture content)
		// for determining if active crown fire possible
		//
		// 1. Calculate canopy bulk density (CBD) at 1 meter intervals from 1 hour fuels (kg m-3).
		// 2. Determine canopy base height (cbht) and canopy height (cht) based on meeting minimum
		// CBD threshold of 0.012 kg m-3 (Reinhardt, Lutes, and Scott 2006 RMRS P 41).
		// 3. Calculate average CBD for the canopy (from cbht to cht).
		// 4. Calculate canopy foliar moisture content (cfm)
		// 5. Determine if conditions for crown fire spread are met

		// 1. Calculate canopy bulk density (CBD) at 1 meter intervals from 1 hour fuels (kg m-3).
		// declare and initialize
		double cbdlevels[80];
		for (i = 0; i < 81; i++)
		{
			cbdlevels[i] = 0.0;
			// if ((i == 10) && date.islastday && date.islastmonth) {
			// dprintf("cbdlelels[i] %f\n",cbdlevels[i]);
			// }
		}

		// *** Loop through PFTs ***
		pftlist.firstobj();
		while (pftlist.isobj)
		{

			Pft &pft = pftlist.getobj();
			Vegetation &vegetation = patch.vegetation;
			Patchpft &patchpft = patch.pft[pft.id];

			vegetation.firstobj();
			while (vegetation.isobj)
			{
				Individual &indiv = vegetation.getobj();

				i = 0;

				if ((indiv.pft.lifeform == TREE) && (indiv.pft.id == pft.id))
				{
                    // added by weichao
					// int proxy_height, proxy_boleht;
					// proxy_height = int(round(indiv.height));
					// proxy_boleht = int(round(indiv.boleht));
					for (i = 0; i < (indiv.height + 1); i++)
					{ 
				        
						if (i >= indiv.boleht)
						{

							// Add CBD from this individual to this level
							//  revised by weichao
							if (indiv.crownlength > 0.0)
							{
								cbdlevels[i] += (c2om * (0.04725 * (indiv.cmass_heart + indiv.cmass_sap) + indiv.cmass_leaf)) / (indiv.crownlength);
							}
							// cbdlevels[i] += (c2om * (0.04725 * (indiv.cmass_heart + indiv.cmass_sap) + indiv.cmass_leaf))
							// / (indiv.crownlength);
							// end of revision by weichao
						}
                                if(date.year-nyear_spinup == 20 && date.month == 7){
									dprintf("i %d indiv.height %f indiv.boleht %f indiv.crownlength %f cbdlevels[i] %f \n",
										i, indiv.height,indiv.boleht,indiv.crownlength,cbdlevels[i]);
								}
								
					}
				} // end if TREE
				// ... on to next individual
				vegetation.nextobj();
			} // end cohort loop

			// ... on to next PFT
			pftlist.nextobj();
		} // end pft loop

		// 2. Determine canopy base height (cbht) and canopy height (cht) based on meeting minimum
		// CBD threshold of 0.012 kg m-3 (Reinhardt, Lutes, and Scott 2006 RMRS P 41).

		// Find the canopy base height, lowest height that CBD is above threshold (default 0.012 kg m-3)
		// double cbht; //canopy base height

		for (i = 0; i < 81; i++)
		{
			if (cbdlevels[i] > 0.012)
			{
				cbht = i;
                if(date.year-nyear_spinup == 20 && date.month == 7){
					dprintf("Canopy Base Height = %.2f\n",cbht);				
			    }
				
				goto endloop;
			}
		}
	endloop: // now you're here but still inside function

		// Find the canopy height, lowest height that CBD is above threshold
		// double cht; //canopy height

		for (i = 80; i >= 0; --i)
		{
			if (cbdlevels[i] > 0.012)
			{
				cht = i - 1; //-1 because need bottom of level to meet threshold
				if(date.year-nyear_spinup == 20 && date.month == 7){
					dprintf("canopy height = %.2f\n",cht);				
				}
				
				goto finish;
			}
		}
	finish: // now you're here but still inside function

		// 3. Calculate average CBD for the canopy (from cbht to cht).
		// double cbd;				//canopy bulk density; average bulk density from cbht to cht
		// double cbdavg;			//average bulk density from cbht to cht (kg m-3)

		cbd = 0.0;

		for (i = cbht; i <= cht; i++)
		{
			cbd += cbdlevels[i];
		}

		if(cht > cbht){
			cbdavg = cbd / (cht - cbht); // get average
		}
		
        if(date.year-nyear_spinup == 20 && date.month == 7){
			dprintf("Canopy Bulk Density = %.2f\n",cbd);
		    dprintf("Average Canopy Bulk Density = %.2f\n",cbdavg);						
		}


		// 4. Calculate canopy foliar moisture content (cfm)
		// Assume canopy foliar moisture content range 50% - 150%
		// soil.dwcontlower[day] //fraction of available water holding capacity

        // revised by weichao
		// cfm = 1.0 * soil.dwcontlower[date.day];
		cfm = soil.dwcontlower[date.day] + 0.5; // previous number
        if(date.year-nyear_spinup == 20 && date.month == 7){
			dprintf("Canopy Foliar Moisture from dwcont = %.2f\n",cfm);						
		}
		

		// TESTING JULY - alternative to calcuate cfm
		// ARVE-Reserach/LPJ-LMfire version 1.3 on GitHub
		// *** Loop through PFTs ***
		pftlist.firstobj();
		while (pftlist.isobj)
		{

			Pft &pft = pftlist.getobj();
			Vegetation &vegetation = patch.vegetation;
			Patchpft &patchpft = patch.pft[pft.id];

			if (pft.lifeform == TREE)
			{

				lfuelw0 += livefuel[pft.id][0]; //! mass of 1-hr fuel in living biomass  !FLAG maximum leaf dry mass set to 8 kg m-2!
			}

			// Loop through cohorts
			vegetation.firstobj();
			while (vegetation.isobj)
			{
				Individual &indiv = vegetation.getobj();

				if ((indiv.pft.lifeform == TREE) && (indiv.pft.id == pft.id) && treecover > 0.0)
				{
                    // revised by weichao
					canopywater += (patch.pft[pft.id].wscal * indiv.fpc) / treecover;
					// canopywater += (indiv.wscal * indiv.fpc) / treecover;
					if(date.year-nyear_spinup == 20 && date.month == 7){
			           dprintf("canopywater = %f indiv.wscal = %f indiv.fpc = %f treecover = %f\n",
					   canopywater, patch.pft[pft.id].wscal, indiv.fpc, treecover);						
		            }
					// canopywater = sum(wscal * fpc_grid, mask=pft%tree) / treecover;
				}

				vegetation.nextobj();
			} // end vegetation loop

			// ... on to next PFT
			pftlist.nextobj();
		} // end pft loop

		cfm = canopywater;
        if(date.year-nyear_spinup == 20 && date.month == 7){
			dprintf("Canopy Foliar Moisture from wscal = %.2f\n", cfm);						
		}
		

		// 5. Determine if conditions for crown fire spread are met
		// Minimum CBD threshold - default 0.12 kg m -3 ADD REFERENCE
		// Maximum cfm threshold - default 1.0 (100%)

		if ((cbdavg > 0.10) && (cfm <= 0.8))
		{ // 0.12 CBD 1.0 cfm TESTING JULY
			criticalROS = true;
			if(date.year-nyear_spinup == 20 && date.month == 7){
				dprintf("criticalROS = true CBD = %.2f CFM = %.2f\n",cbdavg,cfm);						
			}
			
		}
		else
		{
			criticalROS = false;
			if(date.year-nyear_spinup == 20 && date.month == 7){
				dprintf("criticalROS = false CBD = %.2f CFM = %.2f\n",cbdavg,cfm);					
			}
			
		}

	} // End of new code @KE crown fire modeling
     
	// test by weichao
	// criticalROS = false;


	// if (date.islastday && date.islastmonth) {
	// dprintf("Canopy Foliar Moisture from wscal = %.2f\n",cfm);
	// }

	//////////////////////////////////////////////////////////////////////////////////////////////

	//!---
	//! if there is fire today, update the fractional area burned and the live and dead biomass pools

	// afire_frac   !fraction of the gridcell burned > CHANGE add when add human caused fires
	// afire_frac = afire_frac + Abfrac;
	double afire_frac; //! fraction of the gridcell burned
	afire_frac = Abfrac;

// test by weichao	
	pftlist.firstobj();
	while (pftlist.isobj)
	{

		Pft &pft = pftlist.getobj();
		Vegetation &vegetation = patch.vegetation;
		Patchpft &patchpft = patch.pft[pft.id];

		vegetation.firstobj();
		while (vegetation.isobj)
		{
			Individual &indiv = vegetation.getobj();

			if (indiv.pft.id == pft.id)
			{
                // indiv.cmass_sap += Abfrac;

			}

			// ... on to next individual
			vegetation.nextobj();
		} // end cohort loop

		// ... on to next PFT
		pftlist.nextobj();
	} // end pft loop
// end of test by weichao


	// dprintf("BURNDAY Abfrac %.2f\n",Abfrac);

	//!-------------------------------------------
	//! update litter pools to remove burned litter

	// real(sp), pointer, dimension(:) :: litter_ag_fast  !aboveground litter, fast turnover (g C m-2) patch.pft[p].litter_leaf
	// real(sp), pointer, dimension(:) :: litter_ag_slow  !aboveground litter, slow turnover (g C m-2) patch.pft[i].litter_wood
	//  * 1000 convert kg to g

	// add some part of soil to litter_wood and litter_leaf
	// patch.pft[i].litter_wood = (patch.pft[i].litter_sap + patch.pft[i].litter_heart + patch.soil.sompool[SURFFWD].cmass/npft + patch.soil.sompool[SURFCWD].cmass/npft);
	// patch.pft[i].litter_leaf = (patch.pft[i].litter_leaf + patch.soil.sompool[SURFMETA].cmass/npft + patch.soil.sompool[SURFSTRUCT].cmass/npft);
	// revised by weichao
    if(std::isnan(Abfrac)){dprintf("warning--------------Abfrac problem\n");}
	if(std::isnan(CF[0])){CF[0] = 0.0;}
	if(std::isnan(CF[1])){CF[1] = 0.0;}
    if(std::isnan(CF[2])){CF[2] = 0.0;}
    if(std::isnan(CF[3])){CF[3] = 0.0;}
    if(std::isnan(CF[4])){CF[4] = 0.0;}	
	
	for (i = 0; i < npft; i++)
	{
		BBdead[i][0] = Abfrac * CF[0] * (patch.pft[i].litter_leaf + patch.soil.sompool[SURFMETA].cmass / npft + patch.soil.sompool[SURFSTRUCT].cmass / npft) * 1000; //! 1-hr fast litter (leaves and grass)

		BBdead[i][1] = Abfrac * CF[0] * (patch.pft[i].litter_sap + patch.pft[i].litter_heart + patch.soil.sompool[SURFFWD].cmass / npft + patch.soil.sompool[SURFCWD].cmass / npft) * 0.04955 * 1000; //! 1-hr slow litter (twigs)
		BBdead[i][2] = Abfrac * CF[1] * (patch.pft[i].litter_sap + patch.pft[i].litter_heart + patch.soil.sompool[SURFFWD].cmass / npft + patch.soil.sompool[SURFCWD].cmass / npft) * 0.075 * 1000;	  //! 10-hr
		BBdead[i][3] = Abfrac * CF[2] * (patch.pft[i].litter_sap + patch.pft[i].litter_heart + patch.soil.sompool[SURFFWD].cmass / npft + patch.soil.sompool[SURFCWD].cmass / npft) * 0.21 * 1000;	  //! 100-hr
		BBdead[i][4] = Abfrac * CF[3] * (patch.pft[i].litter_sap + patch.pft[i].litter_heart + patch.soil.sompool[SURFFWD].cmass / npft + patch.soil.sompool[SURFCWD].cmass / npft) * 0.6655 * 1000;  //! 1000-hr

		if (date.year - nyear_spinup == 120)
		{
			dprintf("BBdead[%d][0] = %f BBdead[%d][1] = %f BBdead[%d][2] = %f BBdead[%d][3] = %f BBdead[%d][4] = %f Abfrac = %f CF[0] = %f CF[1] = %f CF[2] = %f CF[3] = %f CF[4] = %f\n",
					i, BBdead[i][0], i, BBdead[i][1], i, BBdead[i][2], i, BBdead[i][3], i, BBdead[i][4], Abfrac, CF[0], CF[1], CF[2], CF[3], CF[4]);
		}
	}
	// end of revision by weichao







	for (i = 0; i < npft; i++)
	{
		for (j = 0; j < 5; j++)
		{   
	        if(std::isnan(BBdead[i][j])){BBdead[i][j] = 0.0;}
			annBBdead[i][j] += BBdead[i][j];
		}
	}

	/////////////////////////////////////////////////////////////////////////////////////////
	//!-----------------------
	//! part 2.2.6 fire damage to living plants
	double woiCF_sum = 0.;

	//!---
	//! fire residence time (min) tau_l, Peterson & Ryan 1986, eqn. 8, used conversion factor because original eqn. needs fuel load in g cm-2
	for (j = 0; j < 4; j++)
	{
		woiCF_sum += woi[j] * (1 - sqrt(1 - CF[j]));
	}

	tau_l = 19.7 * woiCF_sum * 0.0001; // TESTING
	// tau_l = 39.4 * woiCF_sum * 0.0001;

	// Peterson & Ryan 1986 "the duration of surface fires in natural fuels is therefore assumed to be less than 8 mins." @KE adds limit
	tau_l = min(tau_l, 8.);

	//!----------------------------------------------
	//! tree mortality due to crown and cambial damage

	// Call firemortality to calculate fraction of crown scorch and total probability of mortality for each PFT
	// if(date.year > 990){
	// if (grasscover >= 0.6){ //TESTING!
	//	dprintf("grasscover >= 0.6, %f, treecover %f date.year %d\n",grasscover,treecover,date.year);
	//}
	// else {
	// dprintf("grasscover < 0.6, %f, treecover %f date.year %d\n",grasscover,treecover,date.year);
	//}
	//}
	firemortality(patch, pftlist);
	// Output:
	// indiv.CK;	// fraction of crown scorch (fraction) for each cohort - @KE adaption for LPJ-GUESS
	// indiv.Pm; 	// total probability of mortality for each cohort - @KE adaption for LPJ-GUESS

	// *** Loop through PFTs ***
	double pre_vege_total_cmass, aft_vege_total_cmass;
	pre_vege_total_cmass = aft_vege_total_cmass = 0.0;
	pftlist.firstobj();
	while (pftlist.isobj)
	{

		Pft &pft = pftlist.getobj();
		Vegetation &vegetation = patch.vegetation;
		Patchpft &patchpft = patch.pft[pft.id];

		// dprintf("\nLMfire() CKbar %f Pmbar %f pft.id %d \n",pft.CKbar, pft.Pmbar,pft.id);

		// Loop through cohorts
		vegetation.firstobj();
		while (vegetation.isobj)
		{
			Individual &indiv = vegetation.getobj();
			// dprintf("ALL indiv.pft.lifeform %d (NOLIFEFORM, TREE, GRASS, CROP) \n",indiv.pft.lifeform);

			if ((indiv.pft.lifeform == TREE) && (indiv.pft.id == pft.id))
			{
				// dprintf("  TREE? indiv.pft.lifeform %d (NOLIFEFORM, TREE, GRASS, CROP) \n",indiv.pft.lifeform); //conditional works
				// TESTING
				//  dprintf("\nLMfire() indiv.CK %f indiv.P_m %f pft.id %d \n",indiv.CK, indiv.P_m, pft.id);

				//!---
				//! calculate combusted live biomass (determined by CK)
				//! the equations below reflect removal of 100% of leaves and 1-h wood and 5% of 10-h wood as written on the next line
				//! sm_ind = sm_ind - (CK * 0.045 * sm_ind + CK * 0.05 * 0.075 * sm_ind)
				//! NB this relationship is from Appendix B, and does not seem to be the same as what is written on
				//! pg 1997 after eqn. 17

				// // added by weichao
				// double pre_vege_total_cmass,burn_amount, aft_vege_total_cmass;
				// pre_vege_total_cmass = burn_amount = aft_vege_total_cmass = 0.0;
				pre_vege_total_cmass += (indiv.cmass_leaf + indiv.cmass_sap + indiv.cmass_heart);
				// if (negligible(indiv.densindiv)) {
				// vegetation.killobj();
				// killed=true;
				// continue;
				// }
				// // end of addition by weichao
				// * 1000 convert kg to g
				// TESTING SEPTEMBER - combustion percentages for 1, 10, 100 hour live fuels from FireBGC v 2.0
				//  indiv.CK = 1.0;
				//  Abfrac = 1.0;
				
				// test by weichao
				if(std::isnan(Abfrac)){
					dprintf("warning--------------Abfrac problem\n");
					}
				if(std::isnan(indiv.CK)){
					dprintf("warning--------------indiv.CK problem\n");
					indiv.CK = 0.0;}
				if(std::isnan(indiv.cmass_heart) || std::isnan(indiv.cmass_leaf) || std::isnan(indiv.cmass_sap)){
					dprintf("warning--------------indiv problem\n");
				}
				BBlive[indiv.pft.id][0] += Abfrac * indiv.CK * (0.04725 * (indiv.cmass_heart + indiv.cmass_sap) + indiv.cmass_leaf) * 0.90 * 1000; // 1-h fuel class, 90% combusted
				BBlive[indiv.pft.id][1] += Abfrac * indiv.CK * (0.075 * (indiv.cmass_heart + indiv.cmass_sap)) * 0.80 * 1000;					   // 10-h fuel class, 80% combusted
				BBlive[indiv.pft.id][2] += Abfrac * indiv.CK * (0.21 * (indiv.cmass_heart + indiv.cmass_sap)) * 0.50 * 1000;					   // 100-h fuel class, 50% combusted

				
				// BBlive[indiv.pft.id][0] += Abfrac * indiv.CK * (0.04725 * (indiv.cmass_heart + indiv.cmass_sap) + indiv.cmass_leaf) * 0.90 * 1000; // 1-h fuel class, 90% combusted
				// BBlive[indiv.pft.id][1] += Abfrac * indiv.CK * (0.075 * (indiv.cmass_heart + indiv.cmass_sap)) * 0.80 * 1000;					   // 10-h fuel class, 80% combusted
				// BBlive[indiv.pft.id][2] += Abfrac * indiv.CK * (0.21 * (indiv.cmass_heart + indiv.cmass_sap)) * 0.50 * 1000;					   // 100-h fuel class, 50% combusted

				// if (date.year - nyear_spinup == 120)
				// {
					// dprintf("patch.managed = %d BBlive[%d][0] = %f BBlive[%d][1] = %f BBlive[%d][2] = %f Abfrac = %f indiv.CK = %f indiv.cmass_sap = %f\n",
							// patch.managed, indiv.pft.id, BBlive[indiv.pft.id][0], indiv.pft.id, BBlive[indiv.pft.id][1], indiv.pft.id, BBlive[indiv.pft.id][2], Abfrac, indiv.CK, indiv.cmass_sap);
				// }
				// // added by weichao
				// burn_amount = Abfrac * indiv.CK * (0.04725 * (indiv.cmass_heart + indiv.cmass_sap) + indiv.cmass_leaf) * 0.90 + \
	// Abfrac * indiv.CK * (0.075 * (indiv.cmass_heart + indiv.cmass_sap)) * 0.80 + \
	// Abfrac * indiv.CK * (0.21  * (indiv.cmass_heart + indiv.cmass_sap)) * 0.50;

				// if (pre_vege_total_cmass > 0.0) {
				// indiv.reduce_biomass(100 * burn_amount / pre_vege_total_cmass, 100 * burn_amount / pre_vege_total_cmass);
				// }
				// // end of addition by weichao

				//    !---
				//    !calculate running sum of mortality probability times burned fraction

				indiv.day_kill = indiv.P_m * Abfrac;

				// dprintf("indiv.day_kill %f = indiv.P_m %f * Abfrac %f, indiv.pft.id %d\n",indiv.day_kill,indiv.P_m, Abfrac, indiv.pft.id);

				// OUTPUT for tracking mortality due to crown vs. cambial damage
				indiv.day_CKkill = indiv.P_mCK * Abfrac;
				indiv.day_taukill = indiv.P_mtau * Abfrac;

				// dprintf("TREES mortality allocated indiv.P_mCK %f indiv.P_mtau %f indiv.P_m %f pft.id %d indiv.pft.id %d indiv.id %d\n",
				// indiv.P_mCK, indiv.P_mtau, indiv.P_m, pft.id, indiv.pft.id, indiv.id);

			} // end TREE conditional


			else if ((indiv.pft.lifeform == GRASS) && (indiv.pft.id == pft.id))
			{

				//    !update live grass biomass
				// double pre_vege_total_cmass, aft_vege_total_cmass;
				pre_vege_total_cmass += indiv.cmass_leaf;
                if(std::isnan(Abfrac)){dprintf("warning--------------Abfrac problem\n");}
				if(std::isnan(CFlg)){CFlg = 0.0;}
				BBlive[indiv.pft.id][0] += Abfrac * CFlg * indiv.cmass_leaf * 1000;

				// // added by weichao
				// indiv.cmass_leaf -= Abfrac * CFlg * indiv.cmass_leaf;
				// indiv.cmass_leaf = max(indiv.cmass_leaf, 0.0);
				// // end of addition by weichao
				// aft_vege_total_cmass += indiv.cmass_leaf;
				// dprintf("GRASS indiv.cmass_leaf %f indiv.cmass_root %f Abfrac %f CFlg %f indiv.pft.id %d \n",
				// indiv.cmass_leaf, indiv.cmass_root,Abfrac,CFlg,indiv.pft.id); //CFlg
			}
			// if(date.year == 510 && date.day == 250){
			// dprintf("Abfrac %f indiv.CK %f indiv.cmass_heart %f indiv.cmass_sap %f indiv.cmass_leaf %f\n",
			// Abfrac, indiv.CK, indiv.cmass_heart, indiv.cmass_sap, indiv.cmass_leaf);
			// dprintf("BBlive[indiv.pft.id][0] %f indiv.pft.id %d \n", BBlive[indiv.pft.id][0], indiv.pft.id);

			// }

			vegetation.nextobj();
		} // end vegetation loop

		// ... on to next PFT
		pftlist.nextobj();
	} // end pft loop



	//!---
	//! annual burned live biomass accounting
	//! carbon flux accounting

	double sum_BBdead = 0.0;
	double sum_BBlive = 0.0;

	// dprintf("date.year  = %d  sum_BBdead = %f  sum_BBlive = %f\n",date.year,sum_BBdead,sum_BBlive);
	// if (date.islastday && date.islastmonth){

	// dprintf("date.year  = %d  sum_BBdead = %f  sum_BBlive = %f\n",date.year,sum_BBdead,sum_BBlive);
	// }

	for (i = 0; i < npft; i++)
	{
		for (j = 0; j < 3; j++)
		{   
	        if(std::isnan(BBlive[i][j])){BBlive[i][j] = 0.0;}
			annBBlive[i][j] += BBlive[i][j];
			sum_BBlive += BBlive[i][j];
		}
	}
	// if(std::isnan(sum_BBlive)){sum_BBlive = 0.0;}

	for (i = 0; i < npft; i++)
	{
		for (j = 0; j < 5; j++)
		{
			if(std::isnan(BBdead[i][j])){BBdead[i][j] = 0.0;}
			sum_BBdead += BBdead[i][j];
		}
	}
	// if(std::isnan(sum_BBdead)){sum_BBdead = 0.0;}

	BBtot = sum_BBdead + sum_BBlive; //! total C emissions from burning, across all PFTs
	if(std::isnan(BBtot)){dprintf("Warning----------------------------------------BBtot = %f\n",BBtot);}
	// added by weichao
	// if (date.year - nyear_spinup == 30)
	// {
		// dprintf("Year = %d Month = %d Day = %d sum_BBdead = %f sum_BBlive = %f Diff = %f pre_vege = %f aft_vege = %f\n",
				// date.year + 1450, date.month, date.day, sum_BBdead, sum_BBlive, (pre_vege_total_cmass - aft_vege_total_cmass) * 1000, pre_vege_total_cmass, aft_vege_total_cmass);
	// }
	if (date.year-nyear_spinup >200)
	{
		for (i = 0; i < npft; i++)
		{
			for (j = 0; j < 3; j++)
			{
				
				dprintf("Year = %d Month = %d Day = %d patch.managed = %d annBBdead[%d][%d] = %f annBBlive[%d][%d] = %f\n",
						(date.year + 1479), date.month, date.day, patch.managed, i, j, i, j, annBBdead[i][j], annBBlive[i][j]);
			}
		}
	}
	// end of addition by weichao 
	for (i = 0; i < npft; i++)
	{
			for (j = 0; j < 3; j++)
			{   if(std::isnan(BBlive[i][j])) BBlive[i][j] = 0.0;
				patch.annual_fire_veg_flux += BBlive[i][j];

				// dprintf("Year = %d Month = %d Day = %d patch.managed = %d annBBdead[%d][%d] = %f annBBlive[%d][%d] = %f\n",
						// (date.year + 1479), date.month, date.day, patch.managed, i, j, i, j, annBBdead[i][j], annBBlive[i][j]);
			}
			for (j = 0; j < 5; j++)
			{   if(std::isnan(BBdead[i][j])) BBdead[i][j] = 0.0;
				patch.annual_fire_fuel_flux += BBdead[i][j];

				// dprintf("Year = %d Month = %d Day = %d patch.managed = %d annBBdead[%d][%d] = %f annBBlive[%d][%d] = %f\n",
						// (date.year + 1479), date.month, date.day, patch.managed, i, j, i, j, annBBdead[i][j], annBBlive[i][j]);
			}
			
	}
	
	patch.annual_fire_flux = patch.annual_fire_fuel_flux + patch.annual_fire_veg_flux;
	// if(date.year-nyear_spinup >0 && date.day>240){
	    // dprintf("Year = %d Month = %d Day = %d stand.id = %d patch.id = %d patch.managed = %d annual_fire_fuel_flux = %f annual_fire_veg_flux = %f annual_fire_flux = %f\n",
			// (date.year + 1479), date.month, date.day, stand.id, patch.id, patch.managed,patch.annual_fire_fuel_flux, patch.annual_fire_veg_flux, patch.annual_fire_flux);
			
	// }
	
		// added by weichao------------------------------------------------------------------------------------------------------------------


	double pre_litter_sap[npft], pre_litter_heart[npft], pre_SURFMETA, pre_SURFSTRUCT, pre_SURFFWD, pre_SURFCWD, pre_fuel_total;
	// double pre_fuel_total;
	pre_fuel_total = 0.0;
	for (i = 0; i < npft; i++)
	{   
		pre_litter_sap[i] = patch.pft[i].litter_sap;
		pre_litter_heart[i] = patch.pft[i].litter_heart;
		pre_fuel_total += patch.pft[i].litter_leaf + patch.pft[i].litter_sap + patch.pft[i].litter_heart;
	}
	pre_fuel_total += patch.soil.sompool[SURFMETA].cmass + patch.soil.sompool[SURFSTRUCT].cmass + patch.soil.sompool[SURFFWD].cmass + patch.soil.sompool[SURFCWD].cmass;
	pre_SURFMETA = patch.soil.sompool[SURFMETA].cmass;
	pre_SURFSTRUCT = patch.soil.sompool[SURFSTRUCT].cmass;
	pre_SURFFWD = patch.soil.sompool[SURFFWD].cmass;
	pre_SURFCWD = patch.soil.sompool[SURFCWD].cmass;
	// end of addition by weichao

	//! revised by weichao
	for (i = 0; i < npft; i++)
	{
		if(pre_fuel_total > 0.0){
			patch.pft[i].litter_leaf -= sum_BBdead/1000/pre_fuel_total*patch.pft[i].litter_leaf;
			patch.annual_fire_fuel_flux_confirm += sum_BBdead/1000/pre_fuel_total*patch.pft[i].litter_leaf;
			patch.pft[i].litter_leaf = max(patch.pft[i].litter_leaf, 0.0);
			
			patch.pft[i].litter_sap -= sum_BBdead/1000/pre_fuel_total*patch.pft[i].litter_sap;
			patch.annual_fire_fuel_flux_confirm += sum_BBdead/1000/pre_fuel_total*patch.pft[i].litter_sap;
			patch.pft[i].litter_sap = max(patch.pft[i].litter_sap, 0.0);
			
			patch.pft[i].litter_heart -= sum_BBdead/1000/pre_fuel_total*patch.pft[i].litter_heart;
			patch.annual_fire_fuel_flux_confirm += sum_BBdead/1000/pre_fuel_total*patch.pft[i].litter_heart;
			patch.pft[i].litter_heart = max(patch.pft[i].litter_heart, 0.0);
		}
        
		// dprintf("BEFORE patch.pft[i].litter_leaf %f patch.pft[i].litter_wood %f\n",patch.pft[i].litter_leaf,patch.pft[i].litter_wood);


	}
	if(pre_fuel_total > 0.0){
	patch.soil.sompool[SURFMETA].cmass -= sum_BBdead/1000/pre_fuel_total * patch.soil.sompool[SURFMETA].cmass;
	patch.annual_fire_fuel_flux_confirm += sum_BBdead/1000/pre_fuel_total * patch.soil.sompool[SURFMETA].cmass;
	patch.soil.sompool[SURFMETA].cmass = max(patch.soil.sompool[SURFMETA].cmass, 0.0);
	
	patch.soil.sompool[SURFSTRUCT].cmass -= sum_BBdead/1000/pre_fuel_total * patch.soil.sompool[SURFSTRUCT].cmass;
	patch.annual_fire_fuel_flux_confirm += sum_BBdead/1000/pre_fuel_total * patch.soil.sompool[SURFSTRUCT].cmass;
	patch.soil.sompool[SURFSTRUCT].cmass = max(patch.soil.sompool[SURFSTRUCT].cmass, 0.0);
	
	patch.soil.sompool[SURFFWD].cmass -= sum_BBdead/1000/pre_fuel_total * patch.soil.sompool[SURFFWD].cmass;
	patch.annual_fire_fuel_flux_confirm += sum_BBdead/1000/pre_fuel_total * patch.soil.sompool[SURFFWD].cmass;
	patch.soil.sompool[SURFFWD].cmass = max(patch.soil.sompool[SURFFWD].cmass, 0.0);
	
	patch.soil.sompool[SURFCWD].cmass -= sum_BBdead/1000/pre_fuel_total * patch.soil.sompool[SURFCWD].cmass;
	patch.annual_fire_fuel_flux_confirm += sum_BBdead/1000/pre_fuel_total * patch.soil.sompool[SURFCWD].cmass;
	patch.soil.sompool[SURFCWD].cmass = max(patch.soil.sompool[SURFCWD].cmass, 0.0);	
	}
	//! end of revision by weichao
	
	/////////////////////////////////////////////////
	// Annual accounting of patch level variables

	acflux_fire = BBtot;		 // !carbon flux from biomass burning (g C m-2 d-1)
	patch.annual_fire_flux_confirm += acflux_fire;
	acflux_fireveg = sum_BBlive; // !carbon flux from biomass burning only live vegetation (g C m-2 d-1)

	// if ((date.year - nyear_spinup >= 0) && date.islastday && date.islastmonth){
	// if (date.islastday && date.islastmonth){

	// dprintf("date.year  = %d  BBtot = %f sum_BBdead %f + sum_BBlive %f acflux_fire %f\n",date.year,BBtot,sum_BBdead,sum_BBlive,acflux_fire);
	// }
	// if (date.year - nyear_spinup == 20 & std::isnan(BBtot) == 1){
	// if ((date.year - nyear_spinup == 1) && (date.day == 240)){

	// dprintf("date.year  = %d ---- BBtot = %f sum_BBdead %f + sum_BBlive %f (acflux_fire %f)\n",date.year,BBtot,sum_BBdead,sum_BBlive,acflux_fire);
	// }

	// dprintf("BBtot %f = sum_BBdead %f + sum_BBlive %f (acflux_fire %f)\n",BBtot,sum_BBdead,sum_BBlive,acflux_fire);

	// added by weichao
	// patch.fluxes.report_flux(Fluxes::FIREC,(sum_BBdead / 1000.));
	if(std::isnan(acflux_fire)){dprintf("warning--------------acflux_fire problem\n");}

	patch.fluxes.report_flux(Fluxes::FIREC, (acflux_fire / 1000.0));

	// end of addition by weichao
	// patch.fluxes.acflux_fire += (acflux_fire / 1000.0);		  // convert gC to KgC
	// patch.fluxes.acflux_fireveg += (acflux_fireveg / 1000.0); // convert gC to KgC
	// dprintf("patch.fluxes.acflux_fire %f patchID %d year %d month %d day %d\n",patch.fluxes.acflux_fire,patch.id,date.year,date.month,date.day);
	// dprintf("patch.fluxes.acflux_fireveg %f sum_BBlive %f \n",patch.fluxes.acflux_fireveg, sum_BBlive);

	// @KE added 
	patch.burndays += 1;
	// dprintf("patch.burndays %d\n",patch.burndays);
	patch.totburn += Ab;
	patch.anumfires += numfires;			 // total number of fires over the course of a year
	patch.sumfdi += FDI;					 // FDI summed up over the course of a year (only burndays!)
	patch.burnedf = patch.totburn / area_ha; // Fraction of patch area destroyed by fire
	// if(date.year-nyear_spinup >0 && date.islastmonth){
		
		// dprintf("Year = %d Month = %d Day = %d totbiomass = %f totburn = %f burnedf = %f\n",
				// date.year+1979-500, date.month, date.day, totbiomass,patch.totburn,patch.burnedf);
	    // dprintf("burndays = %d \n", patch.burndays);
		// dprintf("totburn = %f \n", patch.totburn);
		// dprintf("anumfires = %f \n", patch.anumfires);
		// dprintf("sumfdi = %f \n", patch.sumfdi);
		// dprintf("rabarf end of year %f\n",rabarf);
	// }


	// // *** Loop through PFTs ***
	// pftlist.firstobj();
	// while (pftlist.isobj)
	// {

		// // dprintf("\n pftlist \n");
		// Pft &pft = pftlist.getobj();
		// Standpft &standpft = stand.pft[pft.id];

		// for (z = 0; z < 3; z++)
		// {
			// standpft.BBlive += (BBlive[pft.id][z] / 1000.); //@KE added, convert g C to KgC
															// // statndpft.BBlive initalized date.day=0 in dailyaccounting_stand
			// // dprintf("standpft.BBlive %f BBlive[pft.id][z] %f pft.id %d \n",standpft.BBlive,BBlive[pft.id][z],pft.id);
			// //		if (z==0){
			// //		dprintf("BBlive[pft.id][0] %f pft.id %d date.year %d \n",standpft.BBlive,BBlive[pft.id][0],pft.id,date.year);
			// //		}
		// }
		// pftlist.nextobj();
	// }

	/////////////////////////////////////////////////
	// Monthly accounting of patch level variables

	// // // // // patch.mnumfires[date.month] += numfires; // number of fires over the course of a month
	// // // // // if ((patch.mnumfires[date.month] == 0.) && (patch.cumfires > 0))
	// // // // // {
		// // // // // patch.mnumfires[date.month] += patch.cumfires; // add fires for the month that are currently burning
													   // // // // // // dprintf("ADD CURRENT FIRES patch.mnumfires[date.month] %f",patch.mnumfires[date.month]);
	// // // // // }
	// // // // // // dprintf("patch.mnumfires %f += numfires %d\n",patch.mnumfires[date.month],numfires);
	// // // // // patch.mtotburn[date.month] += Ab;	// sum of area burned (ha d-1) over the course of a month
	// // // // // patch.rabarf[date.month] += rabarf; // sum of raw area burned (ha da-1) before limited by patch size

	// // // // // // Summed here, get averaged by /patch.mnumfires in write_output.cpp
	// // // // // patch.ROSf[date.month] += ROSfsurface; //! overall forward rate of spread of fire (m min-1) average over the course of a month
	// // // // // patch.ROSb[date.month] += ROSbsurface; //! backward rate of spread of fire (m min-1) average over the course of a month
	// // // // // patch.Isurf[date.month] += Isurface;   //! surface fire intensity (kW m-1) average over the course of a month
	// // // // // // dprintf("Isurface %f numfires %d cumfires %d patch.mnumfires[date.month] %f date.year %d date.day %d\n",
	// // // // // //		Isurface,numfires,patch.cumfires,patch.mnumfires[date.month],date.year,date.day);
	// // // // // // dprintf("ROSf[m] %f ROSf %f \n",patch.ROSf[date.month],ROSfsurface);

	// // // // // //! trace gas emissions

	// // // // // BBpft = 0.001 * c2om * sum_BBlive + sum_BBdead; //! units kg (dry matter) m-2 //CHANGE I think this is supposed to be BBpft[npft] tracked for each pft

	// // // // // for (i = 0; i < nspec; i++)
	// // // // // {
		// // // // // Mx[i] = 0.1 * BBpft;
		// // // // // // Mx[i] = sum(emfact(:,l) * BBpft)  //!units g (species) m-2 //CHANGE!!!
	// // // // // }

	// Mx = 0; //CHANGE why then reset to 0??

	//!-----------------------
	// Emissions accounting

	// aMx(1) = aMx(1) + Mx(1) //CHANGE add emissions, need aMx as Global variable
	// aMx(2) = aMx(2) + Mx(2)
	// aMx(3) = aMx(3) + Mx(3)
	// aMx(4) = aMx(4) + Mx(4)
	// aMx(5) = aMx(5) + Mx(5)
	// aMx(6) = aMx(6) + Mx(6)

	///////////////////////////////////////////////////////////////////////////////////////
	// BURNED BIOMASS
	// Accounting for burned biomass
	//
	//! several things happen to biomass as a result of fire
	//! 1. dead litter is combusted
	//! 2. live biomass is killed and added to dead litter
	//! 3. live biomass is directly combusted

	// local variables
	double nind_kill; // fraction of PFT killed by fire (per cohort)
	// double height;	  // height of cohort after fire mortality (meters)

	//	!1. consumption of dead biomass
	//	!calculations across PFT vector



	// for (i=0; i<npft; i++){

	// //dprintf("BEFORE patch.pft[i].litter_leaf %f patch.pft[i].litter_wood %f\n",patch.pft[i].litter_leaf,patch.pft[i].litter_wood);

	// patch.pft[i].litter_leaf -= Abfrac * CF[0] * patch.pft[i].litter_leaf; //!1-hr fast litter (leaves and grass)

	// patch.pft[i].litter_wood -= Abfrac * CF[0] * patch.pft[i].litter_wood * 0.045;   //!1-hr slow litter (twigs)
	// patch.pft[i].litter_wood -= Abfrac * CF[1] * patch.pft[i].litter_wood * 0.075;  //!10-hr
	// patch.pft[i].litter_wood -= Abfrac * CF[2] * patch.pft[i].litter_wood * 0.21;   //!100-hr
	// patch.pft[i].litter_wood -= Abfrac * CF[3] * patch.pft[i].litter_wood * 0.67;	//!1000-hr

	// patch.pft[i].litter_leaf = max(patch.pft[i].litter_leaf,0.0);
	// patch.pft[i].litter_wood = max(patch.pft[i].litter_wood,0.0);

	// // added by weichao
	// patch.pft[i].litter_sap -= Abfrac * CF[0] * patch.pft[i].litter_sap * 0.045;   //!1-hr slow litter (twigs)
	// patch.pft[i].litter_sap -= Abfrac * CF[1] * patch.pft[i].litter_sap * 0.075;  //!10-hr
	// patch.pft[i].litter_sap -= Abfrac * CF[2] * patch.pft[i].litter_sap * 0.21;   //!100-hr
	// patch.pft[i].litter_sap -= Abfrac * CF[3] * patch.pft[i].litter_sap * 0.67;	//!1000-hr

	// patch.pft[i].litter_sap = max(patch.pft[i].litter_sap,0.0);

	// patch.pft[i].litter_heart -= Abfrac * CF[0] * patch.pft[i].litter_heart * 0.045;   //!1-hr slow litter (twigs)
	// patch.pft[i].litter_heart -= Abfrac * CF[1] * patch.pft[i].litter_heart * 0.075;  //!10-hr
	// patch.pft[i].litter_heart -= Abfrac * CF[2] * patch.pft[i].litter_heart * 0.21;   //!100-hr
	// patch.pft[i].litter_heart -= Abfrac * CF[3] * patch.pft[i].litter_heart * 0.67;	//!1000-hr

	// patch.pft[i].litter_heart = max(patch.pft[i].litter_heart,0.0);

	// //dprintf("AFTER patch.pft[i].litter_leaf %f patch.pft[i].litter_wood %f i %d \n",patch.pft[i].litter_leaf,patch.pft[i].litter_wood,i);
	// }

//	!2. transfer of killed but not consumed live biomass to litter
//@KE updated to do daily accounting, using day_kill instead of ann_kill
// also *day_kill instead of *nind_kill to transfer live biomass to litter

	// *** Loop through PFTs ***
	pftlist.firstobj();
	while (pftlist.isobj) {

		Pft& pft=pftlist.getobj();
		Vegetation& vegetation=patch.vegetation;
		Standpft& standpft=stand.pft[pft.id];

		// Loop through individuals

		vegetation.firstobj();
		while (vegetation.isobj) {
			Individual& indiv=vegetation.getobj();

			if ((indiv.pft.lifeform == TREE) && (indiv.densindiv != 0.0)) {

				//TESTING
				// First update fire killed pools
				if (indiv.pft.id == pft.id) {

					// update
					standpft.fkill += indiv.day_kill *
							(indiv.cmass_leaf + indiv.cmass_sap + indiv.cmass_heart + indiv.cmass_root);

					// OUTPUT for tracking mortality due to crown vs. cambial damage
					standpft.CKkill += indiv.day_CKkill *
							(indiv.cmass_leaf + indiv.cmass_sap + indiv.cmass_heart + indiv.cmass_root);
					standpft.taukill += indiv.day_taukill *
							(indiv.cmass_leaf + indiv.cmass_sap + indiv.cmass_heart + indiv.cmass_root);
					double cmass_tracking;
					cmass_tracking = indiv.cmass_leaf + indiv.cmass_root + indiv.cmass_sap
							+ indiv.cmass_heart - indiv.cmass_debt;
//					dprintf("date.day %d indiv.day_kill %f fkill %f cmass_tracking %f pft.id %d indiv.pft.id %d indiv.id %d\n",
//							date.day,indiv.day_kill,standpft.fkill,cmass_tracking,pft.id,indiv.pft.id,indiv.id);
//					dprintf("indiv.cmass_debt %f\n",indiv.cmass_debt);

					//TESTING

				nind_kill = 0.0; //initialize

				//dprintf("indiv.day_kill %f indiv.densindiv %2.8f \n",indiv.day_kill,indiv.densindiv);
				indiv.densindiv -= indiv.day_kill * indiv.densindiv;
				//dprintf("indiv.day_kill %f indiv.densindiv %2.8f \n",indiv.day_kill,indiv.densindiv);

				// patch.pft[indiv.pft.id].litter_leaf += indiv.day_kill * indiv.cmass_leaf;
				patch.soil.sompool[SURFSTRUCT].cmass += indiv.day_kill * indiv.cmass_leaf;
				patch.annual_killed_veg_flux += indiv.day_kill * indiv.cmass_leaf;
				indiv.cmass_leaf -= indiv.day_kill * indiv.cmass_leaf;

				// patch.pft[indiv.pft.id].litter_sap += indiv.day_kill * indiv.cmass_sap;
				patch.soil.sompool[SURFFWD].cmass += indiv.day_kill * indiv.cmass_sap;
				patch.annual_killed_veg_flux += indiv.day_kill * indiv.cmass_sap;
				// patch.pft[indiv.pft.id].litter_heart += indiv.day_kill * indiv.cmass_heart;
				patch.soil.sompool[SURFCWD].cmass += indiv.day_kill * indiv.cmass_heart;
				patch.annual_killed_veg_flux += indiv.day_kill * indiv.cmass_heart;
				indiv.cmass_sap -= indiv.day_kill * indiv.cmass_sap;
				indiv.cmass_heart -= indiv.day_kill * indiv.cmass_heart;

				// patch.pft[indiv.pft.id].litter_root += indiv.day_kill * indiv.cmass_root;
				patch.soil.sompool[SURFCWD].cmass += indiv.day_kill * indiv.cmass_root;
				patch.annual_killed_veg_flux += indiv.day_kill * indiv.cmass_root;
				indiv.cmass_root -= indiv.day_kill * indiv.cmass_root;

				// reduce cmass_debt @KE added
				indiv.cmass_debt -= indiv.day_kill * indiv.cmass_debt;

				//if (indiv.id == 2125) {
//				dprintf("indiv.day_kill %f indiv.densindiv %2.8f indiv.day_CKkill %f indiv.day_taukill %f"
//						"indiv.id %d pft.id %d date.day %d date.year %d\n",
//						indiv.day_kill, indiv.densindiv, indiv.day_CKkill, indiv.day_taukill,
//						indiv.id, pft.id, date.day, date.year);
				//}

				// Remove this cohort completely if all individuals killed, less than 1 tree per patch OR
				// Remove this cohort completely if height is less than height at establishment
				// for saplings, around breast height in the first year (1.3 m)
				if ((!negligible(indiv.cmass_sap)) && (!negligible(indiv.cmass_leaf))) {
// 				dprintf("height %f = indiv.cmass_sap %f / indiv.cmass_leaf %f / indiv.pft.sla %f * indiv.pft.k_latosa %f / indiv.pft.wooddens %f\n",
// 						height,indiv.cmass_sap,indiv.cmass_leaf,indiv.pft.sla,
// 						indiv.pft.k_latosa,indiv.pft.wooddens);
						
					indiv.height = indiv.cmass_sap / indiv.cmass_leaf / indiv.pft.sla
									* indiv.pft.k_latosa / indiv.pft.wooddens;
									
// 				dprintf("AFTER height %f = indiv.cmass_sap %f / indiv.cmass_leaf %f / indiv.pft.sla %f * indiv.pft.k_latosa %f / indiv.pft.wooddens %f\n",
// 						height,indiv.cmass_sap,indiv.cmass_leaf,indiv.pft.sla,
// 						indiv.pft.k_latosa,indiv.pft.wooddens);
				}
				else {
					indiv.height = 0;
				}

				if (((indiv.densindiv * patcharea) < 1.0) || ((indiv.height < 1.3) && (indiv.age > 0) && ScorchH >= (2.0 * indiv.height))) {
// 					dprintf("cohort killed: indiv.height < 1.3m height %f indiv.age %f indiv.id %d pft.id %d\n",
// 							height,indiv.age,indiv.id,pft.id);
// 					dprintf("cohort killed: indiv.densindiv * patcharea < 1.0 indiv.id %d pft.id %d\n",indiv.id,pft.id);
					
					
					// patch.pft[indiv.pft.id].litter_leaf += indiv.cmass_leaf;
					// patch.pft[indiv.pft.id].litter_root += indiv.cmass_root;
					// patch.pft[indiv.pft.id].litter_sap += indiv.cmass_sap;
					// patch.pft[indiv.pft.id].litter_heart += indiv.cmass_heart;
					patch.soil.sompool[SURFSTRUCT].cmass += indiv.cmass_leaf;
					patch.soil.sompool[SURFFWD].cmass += indiv.cmass_sap;
					patch.soil.sompool[SURFCWD].cmass += indiv.cmass_heart;
					patch.soil.sompool[SURFCWD].cmass += indiv.cmass_root;
					
					patch.annual_killed_veg_flux += indiv.cmass_leaf + indiv.cmass_root + indiv.cmass_sap + indiv.cmass_heart;

					// update
					standpft.fkill += (indiv.cmass_leaf + indiv.cmass_sap + indiv.cmass_heart + indiv.cmass_root);
					// OUTPUT for tracking mortality due to crown vs. cambial damage
					if (indiv.day_taukill > indiv.day_CKkill) {
						standpft.taukill += (indiv.cmass_leaf + indiv.cmass_sap + indiv.cmass_heart + indiv.cmass_root);
					}
					else {
						standpft.CKkill += (indiv.cmass_leaf + indiv.cmass_sap + indiv.cmass_heart + indiv.cmass_root);
					}

					vegetation.killobj(); //removes cohort
				}
				} // end if pft.ids match
			} // end if TREE
			else if ((indiv.pft.lifeform == GRASS) && (indiv.densindiv != 0.0)) {
			
				if (indiv.pft.id == pft.id) {

				// transfer killed grass roots to litter
				// patch.pft[indiv.pft.id].litter_root  += (Abfrac * CFlg * indiv.cmass_root);
				patch.soil.sompool[SURFCWD].cmass += indiv.cmass_root;
				patch.annual_killed_veg_flux += (Abfrac * CFlg * indiv.cmass_root);
				indiv.cmass_root -= (Abfrac * CFlg * indiv.cmass_root);

				}// end if pft.ids match
			} // end if GRASS
			
			// ... on to next individual
			vegetation.nextobj();
		} // end cohort loop

// ... on to next PFT
pftlist.nextobj();
} // end pft loop		


//!------------------------------------------------------------------------------------------

	// // revised by weichao------------------------------------------------------------------------------------------------------------
	// pftlist.firstobj();
	// while (pftlist.isobj)
	// {

		// Pft &pft = pftlist.getobj();
		// Vegetation &vegetation = patch.vegetation;
		// Standpft &standpft = stand.pft[pft.id];

		// // Loop through individuals

		// vegetation.firstobj();
		// while (vegetation.isobj)
		// {
			// Individual &indiv = vegetation.getobj();

			// if ((indiv.pft.lifeform == TREE) && (!negligible(indiv.densindiv)))
			// {

				// // TESTING
				// //  First update fire killed pools
				// if (indiv.pft.id == pft.id)
				// {

					// // update
					// standpft.fkill += indiv.day_kill *
									  // (indiv.cmass_leaf + indiv.cmass_sap + indiv.cmass_heart + indiv.cmass_root);

					// // OUTPUT for tracking mortality due to crown vs. cambial damage
					// standpft.CKkill += indiv.day_CKkill *
									   // (indiv.cmass_leaf + indiv.cmass_sap + indiv.cmass_heart + indiv.cmass_root);
					// standpft.taukill += indiv.day_taukill *
										// (indiv.cmass_leaf + indiv.cmass_sap + indiv.cmass_heart + indiv.cmass_root);
					// double cmass_tracking;
					// cmass_tracking = indiv.cmass_leaf + indiv.cmass_root + indiv.cmass_sap + indiv.cmass_heart - indiv.cmass_debt;
					// //					dprintf("date.day %d indiv.day_kill %f fkill %f cmass_tracking %f pft.id %d indiv.pft.id %d indiv.id %d\n",
					// //							date.day,indiv.day_kill,standpft.fkill,cmass_tracking,pft.id,indiv.pft.id,indiv.id);
					// //					dprintf("indiv.cmass_debt %f\n",indiv.cmass_debt);

					// // TESTING

					// nind_kill = 0.0; // initialize

                    // // if(date.year-nyear_spinup == 30){
						// // dprintf("indiv.day_kill %f indiv.densindiv %2.8f \n",indiv.day_kill,indiv.densindiv);
					// // }

					// double day_kill_fraction;

					// day_kill_fraction = 0.0;
					
					// // if(indiv.day_kill > 0 && indiv.densindiv > 0.0 && ScorchH >= (2.0 * indiv.height)){
						// // day_kill_fraction = 1.0;
					// // }
					
					// if(indiv.day_kill > 0 && (!negligible(indiv.densindiv)) && ScorchH >= indiv.height){

						// day_kill_fraction = 0.9;
					// }
					// else if(indiv.day_kill > 0 && (!negligible(indiv.densindiv)) && ScorchH >= indiv.boleht){

						// day_kill_fraction = 0.5;
					// }
					// else if(indiv.day_kill > 0 && (!negligible(indiv.densindiv)) && ScorchH < indiv.boleht){

						// day_kill_fraction = 0.05;
						
					// }
					
					
					// //added human ignition by weichao
					// // if(date.year-nyear_spinup >= human_ignition_year){
						// // if( ScorchH >= indiv.height){

						   // // day_kill_fraction = 0.9;
					    // // }
					    // // else if(ScorchH >= indiv.boleht){

						     // // day_kill_fraction = 0.4;
					    // // }
					    // // else if(ScorchH < indiv.boleht){

						     // // day_kill_fraction = 0.05;
					    // // }
					// // }
					// // if(date.year-nyear_spinup >= human_ignition_year){
						// // day_kill_fraction = 1.0;
					// // }
					// // end of addition by weichao
					
					// // dprintf("indiv.day_kill %f indiv.densindiv %2.8f \n",indiv.day_kill,indiv.densindiv);

					// // patch.pft[indiv.pft.id].litter_leaf += day_kill_fraction * indiv.cmass_leaf;
					// patch.soil.sompool[SURFSTRUCT].cmass += day_kill_fraction * indiv.cmass_leaf;
					// patch.annual_killed_veg_flux += day_kill_fraction * indiv.cmass_leaf;
					// indiv.cmass_leaf -= day_kill_fraction * indiv.cmass_leaf;
					// // added by weichao
					// indiv.cmass_leaf = max(indiv.cmass_leaf, 0.0);

					// // revised by weichao
					// // patch.pft[indiv.pft.id].litter_wood += indiv.day_kill * (indiv.cmass_sap + indiv.cmass_heart);
					// // patch.pft[indiv.pft.id].litter_sap += day_kill_fraction * indiv.cmass_sap;
					// patch.soil.sompool[SURFFWD].cmass += day_kill_fraction * indiv.cmass_sap;
					// patch.annual_killed_veg_flux += day_kill_fraction * indiv.cmass_sap;
					// // patch.pft[indiv.pft.id].litter_heart += day_kill_fraction * indiv.cmass_heart;
					// patch.soil.sompool[SURFCWD].cmass += day_kill_fraction * indiv.cmass_heart;
					// patch.annual_killed_veg_flux += day_kill_fraction * indiv.cmass_heart;

					// // if(date.year >= 640 && indiv.day_kill >= 0.0) {
					// // dprintf("day_kill = %f indiv.cmass_leaf = %f cmass_sap = %f cmass_heart = %f\n",indiv.day_kill, indiv.cmass_leaf, indiv.cmass_sap, indiv.cmass_heart);
					// // }
					// indiv.cmass_sap -= day_kill_fraction * indiv.cmass_sap;
					// indiv.cmass_heart -= day_kill_fraction * indiv.cmass_heart;
					// // added by weichao
					// indiv.cmass_sap = max(indiv.cmass_sap, 0.0);
					// indiv.cmass_heart = max(indiv.cmass_heart, 0.0);

					// // patch.pft[indiv.pft.id].litter_root += day_kill_fraction * indiv.cmass_root;
					// patch.soil.sompool[SURFSTRUCT].cmass += day_kill_fraction * indiv.cmass_root;
					// patch.annual_killed_veg_flux += day_kill_fraction * indiv.cmass_root;
					// indiv.cmass_root -= day_kill_fraction * indiv.cmass_root;
					// // added by weichao
					// indiv.cmass_root = max(indiv.cmass_root, 0.0);

					// // reduce cmass_debt @KE added
					// indiv.cmass_debt -= day_kill_fraction * indiv.cmass_debt;
					// // added by weichao
					// indiv.cmass_debt = max(indiv.cmass_debt, 0.0); // maybe shouble do like this???

					   // // vegetation.killobj(); //removes cohort 
					// // }
				// } // end if pft.ids match
			// }	  // end if TREE
			// else if ((indiv.pft.lifeform == GRASS) && (!negligible(indiv.densindiv)))
			// {

				// if (indiv.pft.id == pft.id)
				// {

					// // transfer killed grass roots to litter
					// // patch.pft[indiv.pft.id].litter_root += (Abfrac * CFlg * indiv.cmass_root);
					// patch.soil.sompool[SURFCWD].cmass += (Abfrac * CFlg * indiv.cmass_root);
					// patch.annual_killed_veg_flux += (Abfrac * CFlg * indiv.cmass_root);
					// indiv.cmass_root -= (Abfrac * CFlg * indiv.cmass_root);

					// // added by weichao
					// indiv.cmass_root = max(indiv.cmass_root, 0.0);
				// } // end if pft.ids match
			// }	  // end if GRASS

			// // ... on to next individual
			// vegetation.nextobj();
		// } // end cohort loop

		// // ... on to next PFT
		// pftlist.nextobj();
	// } // end pft loop
// // end of revision by weichao---------------------------------------------------------------------------------------------


//!------------------------------------------------------------------------------------------


	//	!3. consumption of live biomass
	//
	//	!direct combustion of live biomass is not handled here for the moment
	//	!in the main spitfire subroutine in the BBlive term which goes into the
	//	!calculation of acflux_fire
	// @KE CHANGED to handle here using calculations of BBlive above

	//	    !NOTE: we don't remove the live grass because the time required to rebuild grass
	//	    !biomass in this version of LPJ is such that it will be unrealistically low
	//	    !in years subsequent to a fire and there is no way to manipulate nind with grass
	// @KE CHANGED added grass consumption because done with LPJFIRE

	// Reduce litter pool according to combustion rates of live biomass
	// Note: Affecting litter pools intead of reducing live biomass pools directly because it throws
	// off the allometry.

	// added by weichao
	double surf_litter_leaf, surf_litter_sap, surf_litter_heart;
	double frac_SURFMETA, frac_SURFSTRUCT, frac_SURFFWD, frac_SURFCWD;
	surf_litter_leaf = surf_litter_sap = surf_litter_heart = 0.0;
	frac_SURFMETA = frac_SURFSTRUCT = frac_SURFFWD = frac_SURFCWD = 0.0;

	for (i=0; i<npft; i++){
	surf_litter_leaf += patch.pft[i].litter_leaf;
	surf_litter_sap += patch.pft[i].litter_sap;
	surf_litter_heart += patch.pft[i].litter_heart;
	}

	// end of addition by weihcao

	for (i=0; i<npft; i++){

	
	patch.soil.sompool[SURFSTRUCT].cmass -= (BBlive[i][0] / 1000.);
	patch.annual_fire_veg_flux_confirm += (BBlive[i][0] / 1000.);
	patch.soil.sompool[SURFSTRUCT].cmass = max(patch.soil.sompool[SURFSTRUCT].cmass,0.);

    if((patch.soil.sompool[SURFFWD].cmass + patch.soil.sompool[SURFCWD].cmass) > 0.0){
	patch.soil.sompool[SURFFWD].cmass -= patch.soil.sompool[SURFFWD].cmass / (patch.soil.sompool[SURFFWD].cmass + patch.soil.sompool[SURFCWD].cmass) * (BBlive[i][1] / 1000. + BBlive[i][2] / 1000.);
	patch.annual_fire_veg_flux_confirm += patch.soil.sompool[SURFFWD].cmass / (patch.soil.sompool[SURFFWD].cmass + patch.soil.sompool[SURFCWD].cmass) * (BBlive[i][1] / 1000. + BBlive[i][2] / 1000.);
	patch.soil.sompool[SURFFWD].cmass = max(patch.soil.sompool[SURFFWD].cmass,0.);

	patch.soil.sompool[SURFCWD].cmass -= patch.soil.sompool[SURFCWD].cmass / (patch.soil.sompool[SURFFWD].cmass + patch.soil.sompool[SURFCWD].cmass) * (BBlive[i][1] / 1000. + BBlive[i][2] / 1000.);
	patch.annual_fire_veg_flux_confirm += patch.soil.sompool[SURFCWD].cmass / (patch.soil.sompool[SURFFWD].cmass + patch.soil.sompool[SURFCWD].cmass) * (BBlive[i][1] / 1000. + BBlive[i][2] / 1000.);
	patch.soil.sompool[SURFCWD].cmass = max(patch.soil.sompool[SURFCWD].cmass,0.);
    }
	
	//  Update litter pools @KE added
	// dprintf("BEFORE Direct Combustion patch.pft[i].litter_leaf %f patch.pft[i].litter_wood %f pft %d\n",
	// patch.pft[i].litter_leaf, patch.pft[i].litter_wood,i);

	// if(surf_litter_leaf > 0.0){
	// patch.pft[i].litter_leaf -= (BBlive[i][0] / 1000.);
	// patch.annual_fire_veg_flux_confirm += (BBlive[i][0] / 1000.);
	// patch.pft[i].litter_leaf = max(patch.pft[i].litter_leaf,0.);

	// }
	// if((surf_litter_sap + surf_litter_heart) > 0.0){
	// patch.pft[i].litter_sap -= surf_litter_sap / (surf_litter_sap + surf_litter_heart) * (BBlive[i][1] / 1000. + BBlive[i][2] / 1000.);
	// patch.annual_fire_veg_flux_confirm += surf_litter_sap / (surf_litter_sap + surf_litter_heart) * (BBlive[i][1] / 1000. + BBlive[i][2] / 1000.);
	// patch.pft[i].litter_sap = max(patch.pft[i].litter_sap,0.);

	// }
	// if((surf_litter_sap + surf_litter_heart) > 0.0){
	// patch.pft[i].litter_heart -= surf_litter_heart / (surf_litter_sap + surf_litter_heart) * (BBlive[i][1] / 1000. + BBlive[i][2] / 1000.);
	// patch.annual_fire_veg_flux_confirm += surf_litter_heart / (surf_litter_sap + surf_litter_heart) * (BBlive[i][1] / 1000. + BBlive[i][2] / 1000.);
	// patch.pft[i].litter_heart = max(patch.pft[i].litter_heart,0.);

	// }
	//! end of revision by weichao

	//  Update litter pools @KE added
	// dprintf("AFTER Direct Combustion BBlive[i][0] %f BBlive[i][1] %f BBlive[i][2] %f patch.pft[i].litter_leaf %f patch.pft[i].litter_wood %f pft %d\n",
	// BBlive[i][0],BBlive[i][1],BBlive[i][2],patch.pft[i].litter_leaf, patch.pft[i].litter_wood,i);

	}
	if(date.year-nyear_spinup >0 && date.day>0){
	    dprintf("Year = %d Month = %d Day = %d stand.id = %d patch.id = %d patch.managed = %d annual_fire_fuel_flux = %f annual_fire_fuel_flux_confirm = %f annual_fire_veg_flux = %f annual_fire_veg_flux_confirm = %f annual_killed_veg_flux = %f annual_fire_flux = %f annual_fire_flux_confirm = %f\n",
			(date.year + first_sim_year - nyear_spinup), date.month, date.day, stand.id, patch.id, patch.managed,patch.annual_fire_fuel_flux, patch.annual_fire_fuel_flux_confirm*1000, patch.annual_fire_veg_flux,  patch.annual_fire_veg_flux_confirm*1000, patch.annual_killed_veg_flux*1000, patch.annual_fire_flux, patch.annual_fire_flux_confirm);
			
	}
	// if (date.year - nyear_spinup >= 0){
	// dprintf("FIRE OCCURRED THIS YEAR LMFIRE DONE %d:",date.year);
	// }
} // end of lmfire()

///////////////////////////////////////////////////////////////////////////////////////
// REFERENCES
//
//	Glassy, J.M., Running, S.W. 1994. Validating diurnal climatology logic of the MT-CLIM model
//		across a climatic gradient in Oregon. Ecological Applications., 4(2), 248-257.
//	Thonicke 2010 BGS
// 	Rothermel 1972
//	Morvan et al., 2008, Figure 13
