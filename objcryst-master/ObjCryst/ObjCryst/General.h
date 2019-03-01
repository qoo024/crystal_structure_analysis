/*  ObjCryst++ Object-Oriented Crystallographic Library
    (c) 2000-2002 Vincent Favre-Nicolin vincefn@users.sourceforge.net
        2000-2001 University of Geneva (Switzerland)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
/*   General.h
*  header file for various global declaration. Most of these declarations
* should be moved to other headers, and this file could eventually disappear.
*
*/

#ifndef _VFN_OBJCRYST_H_
#define _VFN_OBJCRYST_H_


#include <string>
#include <cmath>
#include <utility>

// Restricted pointers (useful for auto-vectorization)
#ifdef __GNUG__
   #define RESTRICT __restrict__
#elif defined(_MSC_VER) || defined(__ICL) // MS and VC++ compiler
   #define RESTRICT __restrict
#else
   #define RESTRICT
#endif

//profiling
#ifdef __MWERKS__
#include <Profiler.h>
#endif

#ifdef PROFILING_ON
#include "Profile/Profiler.h"
#else

#define TAU_PROFILE(name, type, group)
#define TAU_PROFILE_START(var)
#define TAU_PROFILE_TIMER(var, name, type, group)
#define TAU_PROFILE_STOP(var)
#define TAU_PROFILE_INIT(argc, argv)
#define TAU_PROFILE_SET_NODE(node)
#define TAU_PROFILE_SET_CONTEXT(context)
#define TAU_EVENT(event, data)
#define TAU_REPORT_STATISTICS()
#define TAU_REPORT_THREAD_STATISTICS()

#endif

#include "ObjCryst/Quirks/VFNDebug.h"

using namespace std;
/** The namespace which includes all objects (crystallographic and
* algorithmic) in ObjCryst++.
*
* \note It may be a good idea to separate in 3 namespaces for ObjCryst, WXObjCryst,
* and RefinableObj
*/
namespace ObjCryst
{

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

#define DEG2RAD (M_PI/180.)
#define RAD2DEG (180./M_PI)

#define REAL float

// Implemented in IO.cpp
/// Function to convert a substring to a floating point value, imposing a C locale (using '.' as decimal separator).
/// This is used for input/output from data file, which are only using the C locale, contrary to the GUI.
float string2floatC(const string &s);

//######################################################################

/// Type of radiation used. Only neutrons and X-Rays are used so far,
/// electrons would require a very different treatment.
enum RadiationType { RAD_NEUTRON, RAD_XRAY, RAD_ELECTRON};
/// Sample type (not used yet)
enum SampleType { SAMPLE_SINGLE_CRYSTAL, SAMPLE_POWDER};
/// Incident beam characteristics : monochromatic, X-Ray tube with Alpha1 and alpha2,
/// MAD (a few wavelengths-UNUSED YET), DAFS (continuous wavelength range-UNUSED YET)
/// LAUE (UNUSED YET), WAVELENGTH_TOF (neutron Time Of Flight)
enum WavelengthType { WAVELENGTH_MONOCHROMATIC, WAVELENGTH_ALPHA12, WAVELENGTH_TOF,
                      WAVELENGTH_MAD, WAVELENGTH_DAFS, WAVELENGTH_LAUE};
/// Profile type for powder (could it be used fopr single crystals on 2D detectors ?)
enum ReflectionProfileType { PROFILE_GAUSSIAN, PROFILE_LORENTZIAN, PROFILE_PSEUDO_VOIGT,
                             PROFILE_PSEUDO_VOIGT_FINGER_COX_JEPHCOAT,
                             PROFILE_PEARSON_VII };

enum PowderBackgroundInterpType{ POWDER_BACKGROUND_LINEAR,
                                 POWDER_BACKGROUND_CUBIC_SPLINE};

#define XRAY_WAVELENGTH_TO_ENERGY 12398.4

//######################################################################
//  Exception.
/** \brief Exception class for ObjCryst++ library
*
*/
//######################################################################
//:TODO: This should go into VFNDebug.h
class ObjCrystException
{
   public:
      ObjCrystException();
      ObjCrystException(const string & message);
      ~ObjCrystException();

      static bool verbose;
      string message;

   protected:
   private:
};

//######################################################################
/** Print some information for the user during long processes.
*/
void ObjCrystInformUserStdOut(const string &);

//######################################################################
/** \brief Pointer to a function for passing info to the user during or
* after long/important processes (use scarcely!)
*
* This function pointer is by default assigned to ObjCrystInformUserStdOut,
* which outputs the message to the standard output. If a user interface is
* used (eg in Fox), this pointer should be reassigned at the beginning of the
* application to a more user-suitable function.
*/
extern void (*fpObjCrystInformUser)(const string &);

/** Class to compare pairs of objects, with the two objects playing a
* symmetric role.
*/
template <class T> class SymmetricPairCompare
{
   public:
      bool operator()(const pair<T,T> &p1, const pair<T,T> &p2) const
      {
         const T* p1f= &(p1.first);
         const T* p1s= &(p1.second);
         const T* p2f= &(p2.first);
         const T* p2s= &(p2.second);
         if(*p1f < *p1s) { p1s= &(p1.first); p1f= &(p1.second);}
         if(*p2f < *p2s) { p2s= &(p2.first); p2f= &(p2.second);}
         if(*p1f != *p2f) return *p1f < *p2f;
         else return *p1s < *p2s;
      }
};

#ifdef OBJCRYST_GL
/// Print a string in 2D at the current OpenGL position
/// This is actually implemented in wxCryst/wxCrystal.cpp
void crystGLPrint(const string &);
#endif

/// Class to store POV-Ray output options
struct CrystalPOVRayOptions
{
   /// Display limits in reduced coordinates
   REAL mXmin, mXmax, mYmin, mYmax, mZmin, mZmax;
   /// Show labels ?
   bool mShowLabel;
   /// Show hydrogens ?
   bool mShowHydrogens;
};

/// This class only serves to temporarilly set the LC_NUMERIC C locale to "C",
/// in order to use '.' as the decimal separator.
/// Just creating one object of type tmp_C_Numeric_locale will switch to the C locale,
/// and when the object gets destroyed it will restore the old locale.
class tmp_C_Numeric_locale
{// Implemented in SpaceGroup.cpp
    public:
        tmp_C_Numeric_locale();
        ~tmp_C_Numeric_locale();
    private:
        std::string mLocale;
};

}//Namespace

#endif //_VFN_OBJCRYST_H_
