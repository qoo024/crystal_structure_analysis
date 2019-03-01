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
/*
*  header file for the RefinablePar and RefinableObj classes
*
* This is still in early development stages !! Not secure !
*
*/

#ifndef _VFN_WX_ATOM_H_
#define _VFN_WX_ATOM_H_

#include "ObjCryst/wxCryst/wxScatterer.h"
#include "ObjCryst/ObjCryst/Atom.h"
namespace ObjCryst
{
/// wxCryst class for Atoms
class WXAtom: public WXScatterer
{
   public:
      WXAtom(wxWindow *parent, Atom*);
      virtual ~WXAtom();
      void OnChangeScattPow(wxCommandEvent & WXUNUSED(event));
      virtual void UpdateUI(const bool mutexlock=false);
   private:
      Atom* mpAtom;
      WXFieldChoice* mpFieldScattPower;
   DECLARE_EVENT_TABLE()
};

} //namespace

#endif //_VFN_WX_ATOM_H_
