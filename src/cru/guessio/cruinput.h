///////////////////////////////////////////////////////////////////////////////////////
/// \file cruinput.h
/// \brief Input module for the CRU-NCEP data set
///
/// \author Joe Siltberg
/// $Date: 2017-04-05 15:04:09 +0200 (Wed, 05 Apr 2017) $
///
/// Modified by:            Weichao Guo(some codes from Kristen Emmett)
/// Version dated:         2019-05-07
/// Updated:               2021-08-28
///////////////////////////////////////////////////////////////////////////////////////

#ifndef LPJ_GUESS_CRUINPUT_H
#define LPJ_GUESS_CRUINPUT_H

#include "guess.h"
#include "inputmodule.h"
#include <vector>
#include "gutil.h"
#include "globalco2file.h"
#include "spinupdata.h"
#include "cru_ts30.h"
#include "lamarquendep.h"
#include "externalinput.h"

/// An input module for CRU climate data
/** This input module gets climate data from binary archives built from
 *  CRU-NCEP (1901-2015).
 */
class CRUInput : public InputModule {
public:

	/// Constructor
	/** Declares the instruction file parameters used by the input module.
	 */

	void test();

	CRUInput();

	/// Destructor, cleans up used resources
	~CRUInput();

	/// Reads in gridlist and initialises the input module
	/** Gets called after the instruction file has been read */
	void init();

	// weichao revision
/// Help function to readenv, reads in 12 monthly values from a text file
	bool searchmydata(double longitude, double latitude);

	// end of weichao revision


	/// See base class for documentation about this function's responsibilities
	bool getgridcell(Gridcell& gridcell);

	/// See base class for documentation about this function's responsibilities
	bool getclimate(Gridcell& gridcell);

	/// See base class for documentation about this function's responsibilities
	void getlandcover(Gridcell& gridcell);

	/// Obtains land management data for one day
	void getmanagement(Gridcell& gridcell) {management_input.getmanagement(gridcell);}

	// Constants associated with historical climate data set

	/// number of years of historical climate
	static const int NYEAR_HIST = CRU_TS30::NYEAR_HIST;

	/// calendar year corresponding to first year in data set
	static const int FIRSTHISTYEAR = CRU_TS30::FIRSTHISTYEAR;

	/// number of years to use for temperature-detrended spinup data set
	/** (not to be confused with the number of years to spinup model for, which
	 * is read from the ins file)
	 */
	static const int NYEAR_SPINUP_DATA=30;

protected:

	/// Gets monthly ndep values for a given calendar year
	/** To be used by sub-classes that wish to do their own
	 *  distribution of monthly values to daily values.
	 *
	 *  The ndep values returned are for the current gridcell,
	 *  i.e. the gridcell chosen in the most recent call to
	 *  getgridcell().
	 *
	 *  \param calendar_year The calendar (not simulation!) year for which to get ndep
	 *  \param mndrydep      Pointer to array holding 12 doubles
	 *  \param mnwetdep      Pointer to array holding 12 doubles
	 */
	void get_monthly_ndep(int calendar_year,
	                      double* mndrydep,
	                      double* mnwetdep);

private:

	/// Type for storing grid cell longitude, latitude and description text
	struct Coord {

		int id;
		double lon;
		double lat;
	    double elev; //@KE adds elevation 10_8_15
	    double slope; //@KE adds slope 10_8_15
		xtring descrip;
	};
    ListArray_id<Coord> gridlist;

	/// Land cover input module
	LandcoverInput landcover_input;
	/// Management input module
	ManagementInput management_input;

	/// search radius to use when finding CRU data
	double searchradius;

	// /// A list of Coord objects containing coordinates of the grid cells to simulate
	// ListArray_id<Coord> gridlist;

	/// Flag for getgridcell(). True indicates that the first gridcell has not been read yet by getgridcell()
	bool first_call;

	// Timers for keeping track of progress through the simulation
	Timer tprogress,tmute;
	static const int MUTESEC=20; // minimum number of sec to wait between progress messages

	/// Yearly CO2 data read from file
	/**
	 * This object is indexed with calendar years, so to get co2 value for
	 * year 1990, use co2[1990]. See documentation for GlobalCO2File for
	 * more information.
	 */
	GlobalCO2File co2;

	/// Monthly temperature for current grid cell and historical period
	double** hist_mtemp;

	/// Monthly min temperature for current grid cell and historical period
	double** hist_mtmin;
	
	/// Monthly max temperature for current grid cell and historical period
	double** hist_mtmax;
	
	/// Monthly precipitation for current grid cell and historical period
	double** hist_mprec;

	/// Monthly sunshine for current grid cell and historical period
	double** hist_msun;
	
	/// Monthly wind speed for current grid cell and historical period
	double** hist_mwind;
	
	/// Monthly lightning for current grid cell and historical period
	double** hist_mlght;

	/// Monthly frost days for current grid cell and historical period
	double** hist_mfrs;

	/// Monthly precipitation days for current grid cell and historical period
	double** hist_mwet;

	/// Monthly DTR (diurnal temperature range) for current grid cell and historical period
	double** hist_mdtr;

	/// Weichao revision
	int elevation;

	/// Nitrogen deposition forcing for current gridcell
	Lamarque::NDepData ndep;

	/// Spinup data for current grid cell - temperature
	Spinup_data spinup_mtemp;
	/// Spinup data for current grid cell - min temperature
	Spinup_data spinup_mtmin;
	/// Spinup data for current grid cell - max temperature
	Spinup_data spinup_mtmax;	
	/// Spinup data for current grid cell - precipitation
	Spinup_data spinup_mprec;
	/// Spinup data for current grid cell - sunshine
	Spinup_data spinup_msun;
	/// Spinup data for current grid cell - wind speed 
	Spinup_data spinup_mwind; // added by weichao
	/// Spinup data for current grid cell - lightning
	Spinup_data spinup_mlght; // added by weichao

	/// Spinup data for current grid cell - frost days
	Spinup_data spinup_mfrs;
	/// Spinup data for current grid cell - precipitation days
	Spinup_data spinup_mwet;
	/// Spinup data for current grid cell - DTR (diurnal temperature range)
	Spinup_data spinup_mdtr;




	/// Daily temperature for current year
	double dtemp[Date::MAX_YEAR_LENGTH];
	/// Daily min temperature for current year
	double dtmin[Date::MAX_YEAR_LENGTH];         // added by weichao
	/// Daily max temperature for current year
	double dtmax[Date::MAX_YEAR_LENGTH];         // added by weichao
	/// Daily precipitation for current year
	double dprec[Date::MAX_YEAR_LENGTH];
	/// Daily sunshine for current year
	double dsun[Date::MAX_YEAR_LENGTH];
	// Daily diurnal temperature range for current year
	// Daily wind for current year
	double dwind[Date::MAX_YEAR_LENGTH];
	// Daily lightning for current year
	double dlght[Date::MAX_YEAR_LENGTH];         // added by weichao
	double ddtr[Date::MAX_YEAR_LENGTH];
	double dNI[Date::MAX_YEAR_LENGTH];         // added by weichao

	/// Daily N deposition for current year
	double dndep[Date::MAX_YEAR_LENGTH];
};

#endif // LPJ_GUESS_CRUINPUT_H
