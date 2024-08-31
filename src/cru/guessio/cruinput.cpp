///////////////////////////////////////////////////////////////////////////////////////
/// \file cruinput.cpp
/// \brief LPJ-GUESS input module for CRU-NCEP data set
///
/// This input module reads in CRU-NCEP climate data in a customised binary format.
/// The binary files contain CRU-NCEP half-degree global historical climate data
/// for 1901-2015.
///
/// \author Ben Smith
/// $Date: 2017-04-05 15:04:09 +0200 (Wed, 05 Apr 2017) $
/// Modified by:            Weichao Guo(some codes from Kristen Emmett)
/// Version dated:         2019-05-07
/// Updated:               2021-08-28
///
///////////////////////////////////////////////////////////////////////////////////////

#include "config.h"
#include "cruinput.h"

#include "driver.h"
#include "parameters.h"
#include <stdio.h>
#include <utility>
#include <vector>
#include <algorithm>
#include <parallel.h>

#include <sstream> //@KE added
#include <iostream> //@KE added for delete operator
#include <string> //@KE added
#include <vector> //@KE added
#include <stdlib.h> //@KE added for rand function

REGISTER_INPUT_MODULE("cru_ncep", CRUInput)

// Anonymous namespace for variables and functions with file scope
namespace {

xtring file_cru;
xtring file_cru_misc;

// weichao revision
// File names for temperature, precipitation, sunshine and soil code driver files
xtring file_temp, file_prec, file_sun, file_dtr, file_wind, file_lght, file_soil, file_soil_depth; // update on 8/26/2021
// LPJ soil code
int soilcode;
double SOILDEPTH_UPPER;
double SOILDEPTH_LOWER;
//end of weichao revision

/// Interpolates monthly data to quasi-daily values.
// void interp_climate(double* mtemp, double* mprec, double* msun, double* mdtr,
					// double* dtemp, double* dprec, double* dsun, double* ddtr) {
	// interp_monthly_means_conserve(mtemp, dtemp);
	// interp_monthly_totals_conserve(mprec, dprec, 0);
	// interp_monthly_means_conserve(msun, dsun, 0);
	// interp_monthly_means_conserve(mdtr, ddtr, 0);
// }

} // namespace



CRUInput::CRUInput()
	: searchradius(0) {

	  spinup_mtemp;
	  spinup_mprec;
	  spinup_msun;
	  spinup_mwind;  // added by weichao
	  spinup_mlght;  // addad by weichao
	  spinup_mfrs;
	  spinup_mwet;
	  spinup_mdtr;
	// Declare instruction file parameters

	declare_parameter("searchradius", &searchradius, 0, 100,
		"If specified, CRU data will be searched for in a circle");
}
	  // spinup_mtemp(NYEAR_SPINUP_DATA),
	  // spinup_mprec(NYEAR_SPINUP_DATA),
	  // spinup_msun(NYEAR_SPINUP_DATA),
	  // spinup_mwind(NYEAR_SPINUP_DATA),  // added by weichao
	  // spinup_mlght(NYEAR_SPINUP_DATA),  // addad by weichao
	  // spinup_mfrs(NYEAR_SPINUP_DATA),
	  // spinup_mwet(NYEAR_SPINUP_DATA),
	  // spinup_mdtr(NYEAR_SPINUP_DATA) {

void CRUInput::init() {

	// DESCRIPTION
	// Initialises input (e.g. opening files), and reads in the gridlist

	//
	// Reads list of grid cells and (optional) description text from grid list file
	// This file should consist of any number of one-line records in the format:
	//   <longitude> <latitude> [<description>]

	double dlon,dlat;
	bool eof=false;
	xtring descrip;
	// Read list of grid coordinates and store in global Coord object 'gridlist'

	// Retrieve name of grid list file as read from ins file
	//xtring file_gridlist=param["file_gridlist"].str;

        xtring working_directory = param["running_directory"].str;	

	xtring task_path;
	task_path.printf("/run%d/",  GuessParallel::get_rank()+1);
	strcat(working_directory, task_path);

	xtring file_gridlist = param["file_gridlist"].str;
	strcat(working_directory, file_gridlist);
	file_gridlist = working_directory;

	FILE* in_grid=fopen(file_gridlist,"r");
	if (!in_grid) fail("initio: could not open %s for input",(char*)file_gridlist);

	//file_cru=param["file_cru"].str;
	//file_cru_misc=param["file_cru_misc"].str;

	// weichao revision
	file_temp=param["file_temp"].str;
	file_dtr = param["file_dtr"].str;
	file_prec = param["file_prec"].str;
	file_sun = param["file_sun"].str;
        file_wind = param["file_wind"].str; // added by weichao
	file_lght = param["file_lght"].str; // added by weichao
	file_soil = param["file_soil"].str;
	file_soil_depth = param["file_soil_depth"].str;
	// end

	gridlist.killall();
	first_call = true;	

	while (!eof) {

		// Read next record in file
		eof=!readfor(in_grid,"f,f,a#",&dlon,&dlat,&descrip);

		if (!eof && !(dlon==0.0 && dlat==0.0)) { // ignore blank lines at end (if any)
			Coord& c=gridlist.createobj(); // add new coordinate to grid list

			c.lon=dlon;
			c.lat=dlat;
			c.descrip=descrip;
		}
	}


	fclose(in_grid);

	// Read CO2 data from file
    co2.load_file(param["file_co2"].str);
	// Open landcover files
	landcover_input.init();
	// Open management files
	management_input.init();

	date.set_first_calendar_year(first_sim_year - nyear_spinup);
	// Set timers
	tprogress.init();
	tmute.init();

	tprogress.settimer();
	tmute.settimer(MUTESEC);
}


bool CRUInput::searchmydata(double longitude,double latitude) {

	// search data in file for the location input
	FILE* in;
	int i, j;
	double elev;
	double dlon, dlat;
	double mtemp[12];		// monthly mean temperature (deg C)
	double mtmin[12]; // monthly max temperature (deg C) //@KE added for LMFire
    double mtmax[12]; // monthly min temperature (deg C) //@KE added for LMFire
	double mprec[12];		// monthly precipitation sum (mm)
	double msun[12];		// monthly mean percentage sunshine values
    double mlght[12]; // monthly mean daily lightning flashes (km-2 day-1) //@KE added for LMFire
    double mwind[12]; // mean monthly wind speed (m s-1) //@KE added for LMFire	
	bool found;

	// weichao test 
	double mwet[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 }; // number of rain days per month

	double mdtr[12];		// monthly mean diurnal temperature range (oC)
	//for (int m = 0; m < 12; m++) {
		//mdtr[m] = 0.;
		//if (ifbvoc) {
			//dprintf("WARNING: No data available for dtr in sample data set!\nNo daytime temperature correction for BVOC calculations applied.");
		//}
	//}
	for (j = 0; j < NYEAR_HIST; j++) {
		for (i = 0; i < 12; i++) {
			hist_mwet[j][i] = mwet[i];
			//hist_mdtr[j][i] = mdtr[i];
		}
	}
	// end of test

///////////////////////////////////////////////////////////////////////////////////////
// DFLASH CALCULATE DAILY LIGHTNING FLASHES
// Used in LM_Fire.cpp, LMFire - @KE added 2018
// !disaggregate lightning flashes randomly only on days with precip, only if there is lightning in the input file in this month


	// temp
	in = fopen(file_temp, "r");

	if (!in) fail("readenv: could not open %s for input", (char*)file_temp);
	found = false;
	for (j = 0; j < NYEAR_HIST;) {
		readfor(in, "f6.2,f5.2,f5.0,12f4.1", &dlon, &dlat, &elev, mtemp);
		//dprintf("Temperature %g\n", mtemp[0]);

			if (equal(longitude, dlon) && equal(latitude, dlat)) {
				for (i = 0; i < 12; i++) {
					hist_mtemp[j][i] = mtemp[i];
					//dprintf("Find Tmp for (%g,%g,%g,%g)\n",
						//dlon,dlat,elev,hist_mtemp[j][i]);
				}
				j++;
				found = true;
			}

			if (feof(in) && !found){
				dprintf("readenv: could not find record for (%g,%g) in %s",
					longitude, latitude, (char*)file_temp);
				fclose(in);
			return false;
		}
	}
	elevation = elev;
	fclose(in);

	// dtr
	in = fopen(file_dtr, "r");

	if (!in) fail("readenv: could not open %s for input", (char*)file_dtr);
	found = false;
	for (j = 0; j < NYEAR_HIST;) {
		readfor(in, "f6.2,f5.2,f5.0,12f4.1", &dlon, &dlat, &elev, mdtr);
		//dprintf("(%g,%g,%g,%g)\n", dlon, dlat, elev, mdtr[0]);

		if (equal(longitude, dlon) && equal(latitude, dlat)) {
			for (i = 0; i < 12; i++) {
				hist_mdtr[j][i] = mdtr[i];
				hist_mtmin[j][i] = hist_mtemp[j][i] - hist_mdtr[j][i]/2; // added by weichao
				hist_mtmax[j][i] = hist_mtemp[j][i] + hist_mdtr[j][i]/2; // added by weichao
				
				// if(i == 2 && j ==1){
				   // dprintf(" find mtmin-mtmax-mtemp-mdtr for (%g,%g,%g,%g,%g,%g,%g)\n",
					// dlon,dlat,elev,hist_mtmin[j][i],hist_mtmax[j][i],hist_mtemp[j][i],hist_mdtr[j][i]);
				// } // test the input

			}
			j++;
			found = true;
		}

		if (feof(in) && !found) {
			dprintf("readenv: could not find record for (%g,%g) in %s",
				longitude, latitude, (char*)file_dtr);
			fclose(in);
			return false;
		}
	}
	fclose(in);
    
	// calculate the min and max temperature from mean and dtr
	// hist_mtmin = hist_mtemp - hist_mdtr/2; // added by weichao  CUI
	// hist_mtmax = hist_mtemp + hist_mdtr/2; // added by weichao  CUI
	
	// prec
	in = fopen(file_prec, "r");

	if (!in) fail("readenv: could not open %s for input", (char*)file_prec);
	found = false;
	for (j = 0; j < NYEAR_HIST;) {
		readfor(in, "f6.2,f5.2,f5.0,12f4", &dlon, &dlat, &elev, mprec);

		if (equal(longitude, dlon) && equal(latitude, dlat)) {
			for (i = 0; i < 12; i++) {
				hist_mprec[j][i] = mprec[i];
				//dprintf("find prec for (%g,%g,%g)\n",
					//dlon, dlat, hist_mprec[j][i]);
			}
			j++;
			found = true;
		}

		if (feof(in) && !found) {
			dprintf("readenv: could not find record for (%g,%g) in %s",
				longitude, latitude, (char*)file_prec);
			fclose(in);
			return false;
		}
	}
	fclose(in);

	// sun
	in = fopen(file_sun, "r");

	if (!in) fail("readenv: could not open %s for input", (char*)file_sun);
	found = false;
	for (j = 0; j < NYEAR_HIST;) {
		readfor(in, "f6.2,f5.2,f5.0,12f4", &dlon, &dlat, &elev, msun); //I think it can turn to another line
		//dprintf("sun %g\n", msun[0]);
		if (equal(longitude, dlon) && equal(latitude, dlat)) {
			for (i = 0; i < 12; i++) {
				hist_msun[j][i] = msun[i];
				//dprintf("Find sun for (%g,%g,%g)\n",
					//dlon, dlat, hist_msun[j][i]);
			}
			j++;
			found = true;
		}

		if (feof(in) && !found) {
			dprintf("readenv: could not find record for (%g,%g) in %s",
				longitude, latitude, (char*)file_sun);
			fclose(in);
			return false;
		}
	}
	fclose(in);

	// wind
	in = fopen(file_wind, "r");

	if (!in) fail("readenv: could not open %s for input", (char*)file_wind);
	found = false;
	for (j = 0; j < NYEAR_HIST;) {
		readfor(in, "f6.2,f5.2,f5.0,12f4.1", &dlon, &dlat, &elev, mwind);

		if (equal(longitude, dlon) && equal(latitude, dlat)) {
			for (i = 0; i < 12; i++) {
				hist_mwind[j][i] = mwind[i];
				//dprintf("find prec for (%g,%g,%g)\n",
					//dlon, dlat, hist_mprec[j][i]);
			}
			j++;
			found = true;
		}

		if (feof(in) && !found) {
			dprintf("readenv: could not find record for (%g,%g) in %s",
				longitude, latitude, (char*)file_wind);
			fclose(in);
			return false;
		}
	}
	fclose(in);

	// lght
	in = fopen(file_lght, "r");

	if (!in) fail("readenv: could not open %s for input", (char*)file_lght);
	found = false;
	for (j = 0; j < NYEAR_HIST;) {
		readfor(in, "f6.2,f5.2,f5.0,12f4", &dlon, &dlat, &elev, mlght);

		if (equal(longitude, dlon) && equal(latitude, dlat)) {
			for (i = 0; i < 12; i++) {
				hist_mlght[j][i] = mlght[i]/10;
				// dprintf("find lightning for (%g,%g,%g)\n",
					// dlon, dlat, hist_mlght[j][i]);
			}
			j++;
			found = true;
		}

		if (feof(in) && !found) {
			dprintf("readenv: could not find record for (%g,%g) in %s",
				longitude, latitude, (char*)file_lght);
			fclose(in);
			return false;
		}
	}
	fclose(in);
	
	return found;
}


//



void CRUInput::get_monthly_ndep(int calendar_year,
                                double* mndrydep,
                                double* mnwetdep) {

	ndep.get_one_calendar_year(calendar_year,
	                           mndrydep, mnwetdep);
}


// void CRUInput::adjust_raw_forcing_data(double lon,
                                       // double lat,
                                       // double hist_mtemp[NYEAR_HIST][12],
									   // double hist_mtmin[NYEAR_HIST][12], // modified by weichao
									   // double hist_mtmax[NYEAR_HIST][12], // modified by weichao
                                       // double hist_mprec[NYEAR_HIST][12],
                                       // double hist_msun[NYEAR_HIST][12],
									   // double hist_mwind[NYEAR_HIST][12],
									   // double hist_mlght[NYEAR_HIST][12]) {

	// // The default (base class) implementation does nothing here.
// }


bool CRUInput::getgridcell(Gridcell& gridcell) {

	// See base class for documentation about this function's responsibilities

	double soilcode;
    double SOILDEPTH_UPPER, SOILDEPTH_LOWER;
	// soiltype

	//

	// Make sure we use the first gridcell in the first call to this function,
	// and then step through the gridlist in subsequent calls.
	if (first_call) {
		gridlist.firstobj();

		// Note that first_call is static, so this assignment is remembered
		// across function calls.
		first_call = false;
	}
	else gridlist.nextobj();

	if (gridlist.isobj) {

		bool gridfound = false;
		bool LUerror = false;
		double lon;
		double lat;

		while (!gridfound) {

			if (gridlist.isobj) {

				lon = gridlist.getobj().lon;
				lat = gridlist.getobj().lat;
				gridfound = searchmydata(lon,lat);
				
				FILE* in;
	            int j;
				bool found;
	            double soildata;	
	            double dlon, dlat;				
	            in = fopen(file_soil, "r");
	            if (!in) fail("readenv: could not open %s for input", (char*)file_soil);
	            found = false;
	                  for (j = 0; j < 6532;j++) {
		                   readfor(in, "f7.2,f6.2,f2.1", &dlon, &dlat, &soildata); 
		                  //dprintf("(%g,%g,%g)\n", dlon,dlat,soildata);
		                   if (equal(lon, dlon) && equal(lat, dlat)) {
                               soilcode = soildata*10;
			                   dprintf("find soil type (%g,%g,%g) in %s",lon, lat, soilcode);
			                   
							   //continue;
			                   found = true;
							   //return found;
		                       }
					  }

		              if (feof(in) && !found) {
			                  dprintf("readenv: could not find record for (%g,%g,%g,%g,%d) in %s",
				              lon, lat, dlon,dlat,j,(char*)file_soil);
			                  fclose(in);
			                  return false;
		                }

	            fclose(in);
				
				
				// change soil depth
				FILE* in2;
	            int jj;
				bool found2;
	            double soildepth, soildepth_total;	
				//double SOILDEPTH_UPPER, SOILDEPTH_LOWER;
	            double dlon2, dlat2;				
	            in2 = fopen(file_soil_depth, "r");
	            if (!in2) fail("readenv: could not open %s for input", (char*)file_soil_depth);
	            found2 = false;
	                  for (jj = 0; jj < 6532;jj++) {
		                   readfor(in2, "f7.2,f6.2,f5.2", &dlon2, &dlat2, &soildepth); 
		                   //dprintf("(%g,%g,%g)\n", dlon2,dlat2,soildepth*100);
		                   if (equal(lon, dlon2) && equal(lat, dlat2)) {
                               soildepth_total = soildepth*1000;
							   SOILDEPTH_UPPER = soildepth_total/3*5; // used to be 5, change to 4
							   SOILDEPTH_LOWER = soildepth_total/3*2*5;
							   //SOILDEPTH_LOWER = soildepth_total-500;
							   // soildepth_upper is 500 mm;
/* 							   if(soildepth_total<=500){
								   SOILDEPTH_LOWER = 10;
							   } */
			                   dprintf("find soil depth (%g,%g,%g,%g) in %s",lon, lat, SOILDEPTH_UPPER, SOILDEPTH_LOWER);
			                   
							   //continue;
			                   found2 = true;
							   //return found;
		                       }
					  }

		              if (feof(in2) && !found2) {
			                  dprintf("readenv: could not find record for (%g,%g,%g,%g,%d) in %s",
				              lon, lat, dlon2,dlat2,jj,(char*)file_soil_depth);
			                  fclose(in2);
			                  return false;
		                }

	            fclose(in2);				
	//weichao
	//soilcode = 3;
               // soilparameters(gridcell.soiltype,soilcode);
				//gridfound = CRU_TS30::findnearestCRUdata(searchradius, file_cru, lon, lat, soilcode,
					//hist_mtemp, hist_mprec, hist_msun);

				//if (gridfound) // Get more historical CRU data for this grid cell
					//gridfound = CRU_TS30::searchcru_misc(file_cru_misc, lon, lat, elevation,
						//hist_mfrs, hist_mwet, hist_mdtr);

				if (run_landcover && gridfound) {
					LUerror = landcover_input.loadlandcover(lon, lat);
					if (!LUerror)
						LUerror = management_input.loadmanagement(lon, lat);
				}

				if (!gridfound || LUerror) {
					if (!gridfound)
						dprintf("\nError: could not find stand at (%g,%g) in climate data files\n", gridlist.getobj().lon, gridlist.getobj().lat);
					else if (LUerror)
						dprintf("\nError: could not find stand at (%g,%g) in landcover/management data file(s)\n", gridlist.getobj().lon, gridlist.getobj().lat);
					gridfound = false;
					gridlist.nextobj();
				}
			}
			else return false;
		}

		// Give sub-classes a chance to modify the data
		// adjust_raw_forcing_data(gridlist.getobj().lon,
			// gridlist.getobj().lat,
			// hist_mtemp, hist_mtmin, hist_mtmax, hist_mprec, hist_msun,hist_mwind,hist_mlght); // modified by weichao

		// Build spinup data sets
		spinup_mtemp.get_data_from(hist_mtemp);
		spinup_mtmin.get_data_from(hist_mtmin); // added by weichao
		spinup_mtmax.get_data_from(hist_mtmax); // added by weichao
		spinup_mprec.get_data_from(hist_mprec);
		spinup_msun.get_data_from(hist_msun);
		spinup_mwind.get_data_from(hist_mwind); // added by weichao
		spinup_mlght.get_data_from(hist_mlght); // added by weichao


		// Detrend spinup temperature data
		spinup_mtemp.detrend_data();

		// guess2008 - new spinup data sets
		spinup_mfrs.get_data_from(hist_mfrs);
		spinup_mwet.get_data_from(hist_mwet);
		spinup_mdtr.get_data_from(hist_mdtr);

		// We wont detrend dtr for now. Partly because dtr is at the moment only
		// used for BVOC, so what happens during the spinup is not affecting
		// results in the period thereafter, and partly because the detrending
		// can give negative dtr values.
		//spinup_mdtr.detrend_data();


		dprintf("\nCommencing simulation for stand at (%g,%g)", gridlist.getobj().lon,
			gridlist.getobj().lat);
		if (gridlist.getobj().descrip != "") dprintf(" (%s)\n\n",
			(char*)gridlist.getobj().descrip);
		else dprintf("\n\n");

		// Tell framework the coordinates of this grid cell
		gridcell.set_coordinates(gridlist.getobj().lon, gridlist.getobj().lat);

		// Get nitrogen deposition data. 
		/* Since the historic data set does not reach decade 2010-2019,
		 * we need to use the RCP data for the last decade. */
		ndep.getndep(param["file_ndep"].str, lon, lat, Lamarque::RCP60);

		// The insolation data will be sent (in function getclimate, below)
		// as incoming shortwave radiation, averages are over 24 hours

		gridcell.climate.instype = SWRAD_TS;

		// Tell framework the soil type of this grid cell
		soilparameters(gridcell.soiltype, soilcode, SOILDEPTH_UPPER, SOILDEPTH_LOWER);

		// For Windows shell - clear graphical output
		// (ignored on other platforms)

		clear_all_graphs();

		return true; // simulate this stand
	}

	return false; // no more stands
}


void CRUInput::getlandcover(Gridcell& gridcell) {

	landcover_input.getlandcover(gridcell);
	landcover_input.get_land_transitions(gridcell);
}


bool CRUInput::getclimate(Gridcell& gridcell) {

	// See base class for documentation about this function's responsibilities

	double progress;

	Climate& climate = gridcell.climate;

	if (date.day == 0) {

		// First day of year ...

		// Extract N deposition to use for this year,
		// monthly means to be distributed into daily values further down
		double mndrydep[12], mnwetdep[12];
		ndep.get_one_calendar_year(date.year - nyear_spinup + first_sim_year,
		                           mndrydep, mnwetdep);

		//std::cout << "Date.year = " << date.year << ", " << nyear_spinup << ", " << first_sim_year << "\n";
		if (date.year < nyear_spinup) {

			// During spinup period

			//std::cout << "Date.year = " << date.year << ", " << state_year << ", " << restart << ", " << NYEAR_SPINUP_DATA << "\n";
			if(date.year == state_year && restart) {

				int year_offset = state_year % NYEAR_SPINUP_DATA;

				for (int y=0;y<year_offset;y++) {
					spinup_mtemp.nextyear();
					spinup_mtmin.nextyear(); // added by weichao
					spinup_mtmax.nextyear(); // added by weichao
					spinup_mprec.nextyear();
					spinup_msun.nextyear();
					spinup_mwind.nextyear(); // added by weichao
					spinup_mlght.nextyear(); // added by weichao
					spinup_mfrs.nextyear();
					spinup_mwet.nextyear();
					spinup_mdtr.nextyear();
				}
			}

			int m;
			double mtemp[12],mtmin[12],mtmax[12],mprec[12],msun[12],mwind[12],mlght[12]; // modified by weichao         CUI
			double mfrs[12],mwet[12],mdtr[12];

			for (m=0;m<12;m++) {
				mtemp[m] = spinup_mtemp[m];
				mtmin[m] = spinup_mtmin[m]; // added by weichao
				mtmax[m] = spinup_mtmax[m]; // added by weichao
				mprec[m] = spinup_mprec[m];
				msun[m]	 = spinup_msun[m];
				mwind[m] = spinup_mwind[m]; // added by weichao
				mlght[m] = spinup_mlght[m]; // added by weichao

				mfrs[m] = spinup_mfrs[m];
				mwet[m] = spinup_mwet[m];
				mdtr[m] = spinup_mdtr[m];
			}

			// Interpolate monthly spinup data to quasi-daily values
			// interp_climate(mtemp,mprec,msun,mdtr,dtemp,dprec,dsun,ddtr);
            interp_climate(mtemp,mprec,msun,mwind,mtmin,mtmax,mlght,dtemp,dprec,dsun,dwind,dtmin,dtmax,dlght,dNI); // from KE           CUI
			int kk;
			for (kk = 0;kk<365;kk++){
				ddtr[kk] = dtmax[kk] - dtmin[kk]; // added by weichao  CUI

				
			}
			// Only recalculate precipitation values using weather generator
			// if rainonwetdaysonly is true. Otherwise we assume that it rains a little every day.
			if (ifrainonwetdaysonly) {
				// (from Dieter Gerten 021121)
				prdaily(mprec, dprec, mwet, gridcell.seed);
			}

			spinup_mtemp.nextyear();
			spinup_mtmin.nextyear(); // added by weichao
			spinup_mtmax.nextyear(); // added by weichao

			spinup_mprec.nextyear();
			spinup_msun.nextyear();
			spinup_mwind.nextyear(); // added by weichao
			spinup_mlght.nextyear(); // added by weichao

			spinup_mfrs.nextyear();
			spinup_mwet.nextyear();
			spinup_mdtr.nextyear();

		}
		else if (date.year < nyear_spinup + NYEAR_HIST) {

			// Historical period

			// Interpolate this year's monthly data to quasi-daily values
			// interp_climate(hist_mtemp[date.year-nyear_spinup],
				// hist_mprec[date.year-nyear_spinup],hist_msun[date.year-nyear_spinup],
					   // hist_mdtr[date.year-nyear_spinup],
				       // dtemp,dprec,dsun,ddtr);
			// interp_climate(hist_mtemp[date.year-nyear_spinup],
				// hist_mprec[date.year-nyear_spinup],hist_msun[date.year-nyear_spinup],
					   // hist_mdtr[date.year-nyear_spinup],
				       // dtemp,dprec,dsun,ddtr);
			interp_climate(hist_mtemp[date.year-nyear_spinup],
				hist_mprec[date.year-nyear_spinup],hist_msun[date.year-nyear_spinup],
					   hist_mwind[date.year-nyear_spinup],hist_mtmin[date.year-nyear_spinup],hist_mtmax[date.year-nyear_spinup],hist_mlght[date.year-nyear_spinup],
				       dtemp,dprec,dsun,dwind,dtmin,dtmax,dlght,dNI);	  
			int pp;
			for (pp = 0;pp<365;pp++){
				ddtr[pp] = dtmax[pp] - dtmin[pp]; // added by weichao 

				
			}			// Only recalculate precipitation values using weather generator
			// if ifrainonwetdaysonly is true. Otherwise we assume that it rains a little every day.
			if (ifrainonwetdaysonly) {
				// (from Dieter Gerten 021121)
				prdaily(hist_mprec[date.year-nyear_spinup], dprec, hist_mwet[date.year-nyear_spinup], gridcell.seed);
			}
		}
		else {
			// Return false if last year was the last for the simulation
			return false;
		}

		// Distribute N deposition
		distribute_ndep(mndrydep, mnwetdep, dprec, dndep);
		
		// If using LMFIRE call dflash() to get daily lightning
		// added by weichao
		if ((firemode == LMFIRE) || (firemode == LMFIRE_CF)){
			// dflash(); 
		}
	} //end of (date.day == 0) conditional

	// Send environmental values for today to framework

	climate.co2 = co2[first_sim_year + date.year - nyear_spinup];

	climate.temp  = dtemp[date.day];
	
	climate.prec  = dprec[date.day];
	climate.insol = dsun[date.day];


	if ((firemode == LMFIRE) || (firemode == LMFIRE_CF)){
	climate.tmin  = dtmin[date.day]; // added by weichao
	climate.tmax  = dtmax[date.day]; // added by weichao
	climate.wind  = dwind[date.day]; // added by weichao
    climate.NI = dNI[date.day];	
	climate.lght  = dlght[date.day]; // added by weichao	
	}

	// Nitrogen deposition
	climate.dndep = dndep[date.day];

	// bvoc
	if(ifbvoc){
	  climate.dtr = ddtr[date.day];
	}

	// First day of year only ...

	if (date.day == 0) {

		// Progress report to user and update timer

		if (tmute.getprogress()>=1.0) {
			progress=(double)(gridlist.getobj().id*(nyear_spinup+NYEAR_HIST)
				+date.year)/(double)(gridlist.nobj*(nyear_spinup+NYEAR_HIST));
			tprogress.setprogress(progress);
			dprintf("%3d%% complete, %s elapsed, %s remaining\n",(int)(progress*100.0),
				tprogress.elapsed.str,tprogress.remaining.str);
			tmute.settimer(MUTESEC);
		}
	}

	return true;
}


CRUInput::~CRUInput() {

	// Performs memory deallocation, closing of files or other "cleanup" functions.

}


///////////////////////////////////////////////////////////////////////////////////////
// REFERENCES
// Lamarque, J.-F., Kyle, G. P., Meinshausen, M., Riahi, K., Smith, S. J., Van Vuuren,
//   D. P., Conley, A. J. & Vitt, F. 2011. Global and regional evolution of short-lived
//   radiatively-active gases and aerosols in the Representative Concentration Pathways.
//   Climatic Change, 109, 191-212.
// Nakai, T., Sumida, A., Kodama, Y., Hara, T., Ohta, T. (2010). A comparison between
//   various definitions of forest stand height and aerodynamic canopy height.
//   Agricultural and Forest Meteorology, 150(9), 1225-1233
