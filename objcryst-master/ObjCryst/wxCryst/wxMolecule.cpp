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

// wx headers, with or without precompilation
#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif
#include "wx/notebook.h"
#include "wx/minifram.h"
#include <wx/wfstream.h>
#include "wx/progdlg.h"

#include <sstream>
#include <fstream>
#include <algorithm>
#include "ObjCryst/wxCryst/wxMolecule.h"
#include "ObjCryst/RefinableObj/LSQNumObj.h"
#include "ObjCryst/Quirks/Chronometer.h"
#include "ObjCryst/Quirks/VFNStreamFormat.h"

namespace ObjCryst
{

//:TODO: Move this to wxCryst.h
template<class T> T const* WXDialogChooseFromVector(const vector<T*> &reg,wxWindow*parent,
                                                const string &message,int &choice)
{
   wxString *choices = new wxString[reg.size()];
   for(unsigned int i=0;i<reg.size();i++)
      choices[i]= wxString::FromAscii((reg[i]->GetName()).c_str());
   wxSingleChoiceDialog dialog
         (parent,message.c_str(),"Choose",reg.size(),choices,NULL,wxOK | wxCANCEL);
   dialog.SetSize(300,300);
   if(wxID_OK!=dialog.ShowModal()) return 0;
   choice=dialog.GetSelection();
   delete[] choices;
   return reg[choice];
}

template<class T> T * WXDialogChooseFromVector(vector<T*> &reg,wxWindow*parent,
                                                const string &message,int &choice)
{
   wxString *choices = new wxString[reg.size()];
   for(unsigned int i=0;i<reg.size();i++)
      choices[i]= wxString::FromAscii((reg[i]->GetName()).c_str());
   #if wxCHECK_VERSION( 2, 9, 4 )
   wxSingleChoiceDialog dialog
   (parent, wxString::FromAscii(message.c_str()),_T("Choose"),reg.size(),choices,(void**)NULL,wxOK | wxCANCEL);
   #else
   wxSingleChoiceDialog dialog
         (parent, wxString::FromAscii(message.c_str()),_T("Choose"),reg.size(),choices,NULL,wxOK | wxCANCEL);
   #endif
   dialog.SetSize(300,300);
   if(wxID_OK!=dialog.ShowModal()) return 0;
   choice=dialog.GetSelection();
   delete[] choices;
   return reg[choice];
}

template<class T> list<T *> WXDialogChooseMultipleFromVector(vector<T*> &reg,wxWindow*parent,
                                                const string &message)
{
   wxString *choices = new wxString[reg.size()];
   for(unsigned int i=0;i<reg.size();i++)
      choices[i]= wxString::FromAscii((reg[i]->GetName()).c_str());
   wxMultiChoiceDialog_ListBox dialog
         (parent, wxString::FromAscii(message.c_str()),_T("Choose"),reg.size(),choices);
   dialog.ShowModal();
   wxArrayInt choice=dialog.GetSelections();
   list<T*> vChoice;
   for(unsigned int i=0;i<choice.GetCount();++i) vChoice.push_back(reg[choice.Item(i)]);
   delete[] choices;
   return vChoice;
}
template<class T> list<T const*> WXDialogChooseMultipleFromVector(const vector<T*> &reg,wxWindow*parent,
                                                const string &message)
{
   wxString *choices = new wxString[reg.size()];
   for(unsigned int i=0;i<reg.size();i++)
      choices[i]= wxString::FromAscii((reg[i]->GetName()).c_str());
   wxMultiChoiceDialog_ListBox dialog
         (parent, wxString::FromAscii(message.c_str()),_T("Choose"),reg.size(),choices);
   dialog.ShowModal();
   wxArrayInt choice=dialog.GetSelections();
   list<T const*> vChoice;
   for(unsigned int i=0;i<choice.GetCount();++i) vChoice.push_back(reg[choice.Item(i)]);
   delete[] choices;
   return vChoice;
}

// Compress a string by removing a given character
string CompressString(const string &s,const string &c)
{
   string sc=s;
   string::size_type idx=0;
   while(idx!=string::npos)
   {
      idx=sc.find(c);
      if(idx!=string::npos) sc.erase(idx,c.size());
   }
   return sc;
}
// Split
list<string> SplitString(const string &str, const string &separator)
{
   string::size_type idx0=(string::size_type) 0;
   string::size_type idx1=(string::size_type) 0;
   list<string> l;
   while((idx1!=string::npos)&&(idx0<str.size()))
   {
      idx1=str.find(separator,idx0);
      if(idx1==string::npos) l.push_back(str.substr(idx0,idx1));
      if(idx1>idx0) l.push_back(str.substr(idx0,idx1-idx0));
      idx0=idx1+1;
   }
   //for(list<string>::const_iterator pos=l.begin();pos!=l.end();++pos) cout <<*pos<<" / ";
   //cout<<endl;
   return l;
}

////////////////////////////////////////////////////////////////////////
//
//    WXMolScrolledWindow
//
////////////////////////////////////////////////////////////////////////
WXMolScrolledWindow::WXMolScrolledWindow(wxWindow* parent, WXMolecule* pWXMol, long id):
wxGrid(parent,id),mpWXMolecule(pWXMol)
{}
WXMolScrolledWindow::~WXMolScrolledWindow()
{
   mpWXMolecule->NotifyDeleteListWin(this);
}
////////////////////////////////////////////////////////////////////////
//
//    WXMolAtom
//
////////////////////////////////////////////////////////////////////////
static const long ID_MOLATOM_SCATTPOW=WXCRYST_ID();
static const long ID_MOLATOM_NAME=WXCRYST_ID();

BEGIN_EVENT_TABLE(WXMolAtom,wxWindow)
   EVT_BUTTON(ID_MOLATOM_SCATTPOW,    WXMolAtom::OnChangeScattPow)
END_EVENT_TABLE()

WXMolAtom::WXMolAtom(wxWindow *parent, MolAtom*obj):
WXCrystObjBasic(parent),mpMolAtom(obj)
{
   VFN_DEBUG_ENTRY("WXMolAtom::WXMolAtom():"<<obj->GetName(),6)
   mpSizer=new wxBoxSizer(wxHORIZONTAL);
   wxStaticText* label=new wxStaticText(this,-1,_T("Atom"));
   mpSizer->Add(label);
   {
      mpFieldName=new WXFieldString(this, mpMolAtom->GetName(),ID_MOLATOM_NAME,80,true);
      mpSizer->Add(mpFieldName,0,wxALIGN_CENTER);
      mpFieldScattPower=new WXFieldChoice(this,ID_MOLATOM_SCATTPOW,"Type:",60);
      mpSizer->Add(mpFieldScattPower,0,wxALIGN_CENTER);
      mList.Add(mpFieldScattPower);
      if(mpMolAtom->IsDummy())
         mpFieldScattPower->SetValue("Dummy");
      else
         mpFieldScattPower->SetValue(mpMolAtom->GetScatteringPower().GetName());
   }
   {
      WXCrystObjBasic* pFieldX=mpMolAtom->GetMolecule().GetPar(&(mpMolAtom->X())).WXCreate(this);
      mpSizer->Add(pFieldX,0,wxALIGN_CENTER);
      mList.Add(pFieldX);
   }
   {
      WXCrystObjBasic* pFieldY=mpMolAtom->GetMolecule().GetPar(&(mpMolAtom->Y())).WXCreate(this);
      mpSizer->Add(pFieldY,0,wxALIGN_CENTER);
      mList.Add(pFieldY);
   }
   {
      WXCrystObjBasic* pFieldZ=mpMolAtom->GetMolecule().GetPar(&(mpMolAtom->Z())).WXCreate(this);
      mpSizer->Add(pFieldZ,0,wxALIGN_CENTER);
      mList.Add(pFieldZ);
   }

   this->SetSizer(mpSizer);
   this->Layout();
   this->CrystUpdate(true);
   VFN_DEBUG_EXIT("WXMolAtom::WXMolAtom():"<<obj->GetName(),6)
}

WXMolAtom::~WXMolAtom()
{
   mpMolAtom->WXNotifyDelete();
}

void WXMolAtom::CrystUpdate(const bool uui,const bool lock)
{
   VFN_DEBUG_ENTRY("WXMolAtom::CrystUpdate()",5)
   if(lock) mMutex.Lock();
   mList.CrystUpdate(uui,false);
   if(lock) mMutex.Unlock();
   VFN_DEBUG_EXIT("WXMolAtom::CrystUpdate()",5)
}

void WXMolAtom::UpdateUI(const bool lock)
{
   VFN_DEBUG_ENTRY("WXMolAtom::UpdateUI()",5)
   if(lock) mMutex.Lock();
   mList.UpdateUI(false);
   mpFieldName->SetValue(mpMolAtom->GetName().c_str());
   if(mpMolAtom->IsDummy())
      mpFieldScattPower->SetValue("Dummy");
   else
      mpFieldScattPower->SetValue(mpMolAtom->GetScatteringPower().GetName());
   if(lock) mMutex.Unlock();
   VFN_DEBUG_EXIT("WXMolAtom::UpdateUI()",5)
}

void WXMolAtom::OnChangeScattPow(wxCommandEvent & WXUNUSED(event))
{
   VFN_DEBUG_ENTRY("WXMolAtom::OnChangeScattPow()",6)
   WXCrystValidateAllUserInput();
   int choice;
   const ScatteringPower *scatt=WXDialogChooseFromRegistry(
               mpMolAtom->GetMolecule().GetCrystal().GetScatteringPowerRegistry(),
               (wxWindow*)this,"Choose a new Scattering Power",choice);
   if(0==scatt) return;
   mpMolAtom->SetScatteringPower(*scatt);
   this->CrystUpdate(true);
   this->UpdateUI(true);
   VFN_DEBUG_EXIT("WXMolAtom::OnChangeScattPow()",6)
}
////////////////////////////////////////////////////////////////////////
//
//    WXMolBond
//
////////////////////////////////////////////////////////////////////////
WXCRYST_ID ID_MOLBOND_ATOM1;
WXCRYST_ID ID_MOLBOND_ATOM2;
WXCRYST_ID ID_MOLBOND_FREEBUTTON;

BEGIN_EVENT_TABLE(WXMolBond,wxWindow)
   EVT_BUTTON(ID_MOLBOND_ATOM1,    WXMolBond::OnChangeAtom)
   EVT_BUTTON(ID_MOLBOND_ATOM2,    WXMolBond::OnChangeAtom)
   EVT_CHECKBOX(ID_MOLBOND_FREEBUTTON,    WXMolBond::OnToggleFree)
END_EVENT_TABLE()

WXMolBond::WXMolBond(wxWindow *parent, MolBond*obj):
WXCrystObjBasic(parent),mpMolBond(obj),mpButtonFree(0)
{
   VFN_DEBUG_ENTRY("WXMolBond::WXMolBond():"<<obj->GetName(),6)
   mpSizer=new wxBoxSizer(wxHORIZONTAL);
   #if 1
   mpButtonFree=new wxCheckBox(this,ID_MOLBOND_FREEBUTTON,_T(""),wxDefaultPosition, wxDefaultSize);
   mpButtonFree->Fit();
   mpSizer->Add(mpButtonFree,0,wxALIGN_CENTER);
   #endif
   mpFieldAtom1=new WXFieldChoice(this,ID_MOLBOND_ATOM1,"Bond:",60);
   mpFieldAtom1->SetValue(mpMolBond->GetAtom1().GetName());
   mpSizer->Add(mpFieldAtom1,0,wxALIGN_CENTER);
   mList.Add(mpFieldAtom1);

   mpFieldAtom2=new WXFieldChoice(this,ID_MOLBOND_ATOM2,"-",60);
   mpFieldAtom2->SetValue(mpMolBond->GetAtom2().GetName());
   mpSizer->Add(mpFieldAtom2,0,wxALIGN_CENTER);
   mList.Add(mpFieldAtom2);

   WXFieldPar<REAL> *value=
      new WXFieldPar<REAL>(this,"Length=",-1,&mValue);
   mpSizer->Add(value,0,wxALIGN_CENTER);
   mList.Add(value);

   WXFieldPar<REAL> *length=
      new WXFieldPar<REAL>(this,",Restraint=",-1,&(mpMolBond->Length0()));
   mpSizer->Add(length,0,wxALIGN_CENTER);
   mList.Add(length);

   WXFieldPar<REAL> *delta=
      new WXFieldPar<REAL>(this,",delta=",-1,&(mpMolBond->LengthDelta()));
   mpSizer->Add(delta,0,wxALIGN_CENTER);
   mList.Add(delta);

   WXFieldPar<REAL> *sigma=
      new WXFieldPar<REAL>(this,",sigma=",-1,&(mpMolBond->LengthSigma()));
   mpSizer->Add(sigma,0,wxALIGN_CENTER);
   mList.Add(sigma);

   this->SetSizer(mpSizer);
   this->Layout();
   this->CrystUpdate(true,true);
   VFN_DEBUG_EXIT("WXMolBond::WXMolBond():"<<obj->GetName(),6)
}
WXMolBond::~WXMolBond()
{
   mpMolBond->WXNotifyDelete();
}

void WXMolBond::CrystUpdate(const bool uui,const bool lock)
{
   VFN_DEBUG_ENTRY("WXMolBond::CrystUpdate()",5)
   if(lock) mMutex.Lock();
   mValue=mpMolBond->GetLength();
   mList.CrystUpdate(uui,false);
   if(lock) mMutex.Unlock();
   VFN_DEBUG_EXIT("WXMolBond::CrystUpdate()",5)
}

void WXMolBond::UpdateUI(const bool lock)
{
   VFN_DEBUG_ENTRY("WXMolBond::UpdateUI()",5)
   if(lock) mMutex.Lock();
   if(0!=mpButtonFree) mpButtonFree->SetValue(mpMolBond->IsFreeTorsion());
   mList.UpdateUI(false);
   mpFieldAtom1->SetValue(mpMolBond->GetAtom1().GetName().c_str());
   mpFieldAtom2->SetValue(mpMolBond->GetAtom2().GetName().c_str());
   if(lock) mMutex.Unlock();
   VFN_DEBUG_EXIT("WXMolBond::UpdateUI()",5)
}

void WXMolBond::OnChangeAtom(wxCommandEvent &event)
{
   VFN_DEBUG_ENTRY("WXMolBond::OnChangeAtom()",6)
   WXCrystValidateAllUserInput();
   int choice;
   MolAtom *const at=WXDialogChooseFromVector(mpMolBond->GetMolecule().GetAtomList(),
                               (wxWindow*)this,"Choose a new Atom",choice);
   if(0==at) return;
   if((long)ID_MOLBOND_ATOM1==event.GetId())
   {
      if(at==&(mpMolBond->GetAtom2()))
      {
         wxMessageDialog dumbUser(this,_T("The two atoms must be different !"),
                                  _T("Whooops"),wxOK|wxICON_EXCLAMATION);
         dumbUser.ShowModal();
         return;
      }
      mpMolBond->SetAtom1(*at);
   }
   if((long)ID_MOLBOND_ATOM2==event.GetId())
   {
      if(at==&(mpMolBond->GetAtom1()))
      {
         wxMessageDialog dumbUser(this,_T("The two atoms must be different !"),
                                  _T("Whooops"),wxOK|wxICON_EXCLAMATION);
         dumbUser.ShowModal();
         return;
      }
      mpMolBond->SetAtom2(*at);
   }

   this->CrystUpdate(true);
   this->UpdateUI(true);
   VFN_DEBUG_EXIT("WXMolBond::OnChangeScattPow()",6)
}

void WXMolBond::OnToggleFree(wxCommandEvent & WXUNUSED(event))
{
   VFN_DEBUG_MESSAGE("WXMolBond::OnToggleFree()",6)
   if(0!=mpButtonFree) mpMolBond->SetFreeTorsion(mpButtonFree->GetValue());
}

////////////////////////////////////////////////////////////////////////
//
//    WXMolBondAngle
//
////////////////////////////////////////////////////////////////////////
WXCRYST_ID ID_MOLBONDANGLE_ATOM1;
WXCRYST_ID ID_MOLBONDANGLE_ATOM2;
WXCRYST_ID ID_MOLBONDANGLE_ATOM3;

BEGIN_EVENT_TABLE(WXMolBondAngle,wxWindow)
   EVT_BUTTON(ID_MOLBONDANGLE_ATOM1,    WXMolBondAngle::OnChangeAtom)
   EVT_BUTTON(ID_MOLBONDANGLE_ATOM2,    WXMolBondAngle::OnChangeAtom)
   EVT_BUTTON(ID_MOLBONDANGLE_ATOM3,    WXMolBondAngle::OnChangeAtom)
END_EVENT_TABLE()

WXMolBondAngle::WXMolBondAngle(wxWindow *parent, MolBondAngle*obj):
WXCrystObjBasic(parent),mpMolBondAngle(obj)
{
   VFN_DEBUG_ENTRY("WXMolBondAngle::WXMolBond():"<<obj->GetName(),6)
   mpSizer=new wxBoxSizer(wxHORIZONTAL);

   mpFieldAtom1=new WXFieldChoice(this,ID_MOLBONDANGLE_ATOM1,"Bond Angle:",60);
   mpFieldAtom1->SetValue(mpMolBondAngle->GetAtom1().GetName());
   mpSizer->Add(mpFieldAtom1,0,wxALIGN_CENTER);
   mList.Add(mpFieldAtom1);

   mpFieldAtom2=new WXFieldChoice(this,ID_MOLBONDANGLE_ATOM2,"-",60);
   mpFieldAtom2->SetValue(mpMolBondAngle->GetAtom2().GetName());
   mpSizer->Add(mpFieldAtom2,0,wxALIGN_CENTER);
   mList.Add(mpFieldAtom2);

   mpFieldAtom3=new WXFieldChoice(this,ID_MOLBONDANGLE_ATOM3,"-",60);
   mpFieldAtom3->SetValue(mpMolBondAngle->GetAtom3().GetName());
   mpSizer->Add(mpFieldAtom3,0,wxALIGN_CENTER);
   mList.Add(mpFieldAtom3);

   WXFieldPar<REAL> *value=
      new WXFieldPar<REAL>(this,"Angle=",-1,&mValue);
   value->SetHumanValueScale(RAD2DEG);
   mpSizer->Add(value,0,wxALIGN_CENTER);
   mList.Add(value);

   WXFieldPar<REAL> *angle=
      new WXFieldPar<REAL>(this,",Restraint=",WXCRYST_ID(),&(mpMolBondAngle->Angle0()));
   angle->SetHumanValueScale(RAD2DEG);
   mpSizer->Add(angle,0,wxALIGN_CENTER);
   mList.Add(angle);

   WXFieldPar<REAL> *delta=
      new WXFieldPar<REAL>(this,",delta=",WXCRYST_ID(),&(mpMolBondAngle->AngleDelta()));
   delta->SetHumanValueScale(RAD2DEG);
   mpSizer->Add(delta,0,wxALIGN_CENTER);
   mList.Add(delta);

   WXFieldPar<REAL> *sigma=
      new WXFieldPar<REAL>(this,",sigma=",WXCRYST_ID(),&(mpMolBondAngle->AngleSigma()));
   sigma->SetHumanValueScale(RAD2DEG);
   mpSizer->Add(sigma,0,wxALIGN_CENTER);
   mList.Add(sigma);

   this->SetSizer(mpSizer);
   this->Layout();
   this->CrystUpdate(true,true);
   VFN_DEBUG_EXIT("WXMolBondAngle::WXMolBond():"<<obj->GetName(),6)
}

WXMolBondAngle::~WXMolBondAngle()
{
   mpMolBondAngle->WXNotifyDelete();
}

void WXMolBondAngle::CrystUpdate(const bool uui,const bool lock)
{
   VFN_DEBUG_ENTRY("WXMolBondAngle::CrystUpdate()",5)
   if(lock) mMutex.Lock();
   mValue=mpMolBondAngle->GetAngle();
   mList.CrystUpdate(uui,false);
   if(lock) mMutex.Unlock();
   VFN_DEBUG_EXIT("WXMolBondAngle::CrystUpdate()",5)
}

void WXMolBondAngle::UpdateUI(const bool lock)
{
   VFN_DEBUG_ENTRY("WXMolBondAngle::UpdateUI()",5)
   if(lock) mMutex.Lock();
   mList.UpdateUI(false);
   mpFieldAtom1->SetValue(mpMolBondAngle->GetAtom1().GetName().c_str());
   mpFieldAtom2->SetValue(mpMolBondAngle->GetAtom2().GetName().c_str());
   mpFieldAtom3->SetValue(mpMolBondAngle->GetAtom3().GetName().c_str());
   if(lock) mMutex.Unlock();
   VFN_DEBUG_EXIT("WXMolBondAngle::UpdateUI()",5)
}

void WXMolBondAngle::OnChangeAtom(wxCommandEvent &event)
{
   VFN_DEBUG_ENTRY("WXMolBondAngle::OnChangeAtom()",6)
   WXCrystValidateAllUserInput();
   int choice;
   MolAtom *const at=WXDialogChooseFromVector(mpMolBondAngle->GetMolecule().GetAtomList(),
                               (wxWindow*)this,"Choose a new Atom",choice);
   if(0==at) return;
   if((long)ID_MOLBONDANGLE_ATOM1==event.GetId())
   {
      if((at==&(mpMolBondAngle->GetAtom2()) )||(at==&(mpMolBondAngle->GetAtom3())) )
      {
         wxMessageDialog dumbUser(this,_T("The three atoms must be different !"),
                                  _T("Whooops"),wxOK|wxICON_EXCLAMATION);
         dumbUser.ShowModal();
         return;
      }
      mpMolBondAngle->SetAtom1(*at);
   }
   if((long)ID_MOLBONDANGLE_ATOM2==event.GetId())
   {
      if((at==&(mpMolBondAngle->GetAtom1()) )||(at==&(mpMolBondAngle->GetAtom3())) )
      {
         wxMessageDialog dumbUser(this,_T("The three atoms must be different !"),
                                  _T("Whooops"),wxOK|wxICON_EXCLAMATION);
         dumbUser.ShowModal();
         return;
      }
      mpMolBondAngle->SetAtom2(*at);
   }
   if((long)ID_MOLBONDANGLE_ATOM3==event.GetId())
   {
      if((at==&(mpMolBondAngle->GetAtom1()) )||(at==&(mpMolBondAngle->GetAtom2())) )
      {
         wxMessageDialog dumbUser(this,_T("The three atoms must be different !"),
                                  _T("Whooops"),wxOK|wxICON_EXCLAMATION);
         dumbUser.ShowModal();
         return;
      }
      mpMolBondAngle->SetAtom3(*at);
   }

   this->CrystUpdate(true);
   this->UpdateUI(true);
   VFN_DEBUG_EXIT("WXMolBondAngle::OnChangeScattPow()",6)
}
////////////////////////////////////////////////////////////////////////
//
//    WXMolDihedralAngle
//
////////////////////////////////////////////////////////////////////////
WXCRYST_ID ID_MOLDIHEDRALANGLE_ATOM1;
WXCRYST_ID ID_MOLDIHEDRALANGLE_ATOM2;
WXCRYST_ID ID_MOLDIHEDRALANGLE_ATOM3;
WXCRYST_ID ID_MOLDIHEDRALANGLE_ATOM4;

BEGIN_EVENT_TABLE(WXMolDihedralAngle,wxWindow)
   EVT_BUTTON(ID_MOLDIHEDRALANGLE_ATOM1,    WXMolDihedralAngle::OnChangeAtom)
   EVT_BUTTON(ID_MOLDIHEDRALANGLE_ATOM2,    WXMolDihedralAngle::OnChangeAtom)
   EVT_BUTTON(ID_MOLDIHEDRALANGLE_ATOM3,    WXMolDihedralAngle::OnChangeAtom)
   EVT_BUTTON(ID_MOLDIHEDRALANGLE_ATOM4,    WXMolDihedralAngle::OnChangeAtom)
END_EVENT_TABLE()

WXMolDihedralAngle::WXMolDihedralAngle(wxWindow *parent, MolDihedralAngle*obj):
WXCrystObjBasic(parent),mpMolDihedralAngle(obj)
{
   VFN_DEBUG_ENTRY("WXMolDihedralAngle::WXMolBond()",6)
   mpSizer=new wxBoxSizer(wxHORIZONTAL);

   mpFieldAtom1=new WXFieldChoice(this,ID_MOLDIHEDRALANGLE_ATOM1,"Dihedral Angle:",60);
   mpFieldAtom1->SetValue(mpMolDihedralAngle->GetAtom1().GetName());
   mpSizer->Add(mpFieldAtom1,0,wxALIGN_CENTER);
   mList.Add(mpFieldAtom1);

   mpFieldAtom2=new WXFieldChoice(this,ID_MOLDIHEDRALANGLE_ATOM2,"-",60);
   mpFieldAtom2->SetValue(mpMolDihedralAngle->GetAtom2().GetName());
   mpSizer->Add(mpFieldAtom2,0,wxALIGN_CENTER);
   mList.Add(mpFieldAtom2);

   mpFieldAtom3=new WXFieldChoice(this,ID_MOLDIHEDRALANGLE_ATOM3,"-",60);
   mpFieldAtom3->SetValue(mpMolDihedralAngle->GetAtom3().GetName());
   mpSizer->Add(mpFieldAtom3,0,wxALIGN_CENTER);
   mList.Add(mpFieldAtom3);

   mpFieldAtom4=new WXFieldChoice(this,ID_MOLDIHEDRALANGLE_ATOM4,"-",60);
   mpFieldAtom4->SetValue(mpMolDihedralAngle->GetAtom4().GetName());
   mpSizer->Add(mpFieldAtom4,0,wxALIGN_CENTER);
   mList.Add(mpFieldAtom4);

   WXFieldPar<REAL> *value=
      new WXFieldPar<REAL>(this,"Angle=",-1,&mValue);
   value->SetHumanValueScale(RAD2DEG);
   mpSizer->Add(value,0,wxALIGN_CENTER);
   mList.Add(value);

   WXFieldPar<REAL> *angle=
      new WXFieldPar<REAL>(this,"Restraint:",-1,&(mpMolDihedralAngle->Angle0()));
   angle->SetHumanValueScale(RAD2DEG);
   mpSizer->Add(angle,0,wxALIGN_CENTER);
   mList.Add(angle);

   WXFieldPar<REAL> *delta=
      new WXFieldPar<REAL>(this,",delta=",-1,&(mpMolDihedralAngle->AngleDelta()));
   delta->SetHumanValueScale(RAD2DEG);
   mpSizer->Add(delta,0,wxALIGN_CENTER);
   mList.Add(delta);

   WXFieldPar<REAL> *sigma=
      new WXFieldPar<REAL>(this,",sigma=",-1,&(mpMolDihedralAngle->AngleSigma()));
   sigma->SetHumanValueScale(RAD2DEG);
   mpSizer->Add(sigma,0,wxALIGN_CENTER);
   mList.Add(sigma);

   this->SetSizer(mpSizer);
   this->Layout();
   this->CrystUpdate(true);
   VFN_DEBUG_EXIT("WXMolDihedralAngle::WXMolBond():"<<obj->GetName(),6)
}

WXMolDihedralAngle::~WXMolDihedralAngle()
{
   mpMolDihedralAngle->WXNotifyDelete();
}

void WXMolDihedralAngle::CrystUpdate(const bool uui,const bool lock)
{
   VFN_DEBUG_ENTRY("WXMolDihedralAngle::CrystUpdate()",5)
   if(lock) mMutex.Lock();
   mValue=mpMolDihedralAngle->GetAngle();
   mList.CrystUpdate(uui,false);
   if(lock) mMutex.Unlock();
   VFN_DEBUG_EXIT("WXMolDihedralAngle::CrystUpdate()",5)
}

void WXMolDihedralAngle::UpdateUI(const bool lock)
{
   VFN_DEBUG_ENTRY("WXMolDihedralAngle::UpdateUI()",5)
   if(lock) mMutex.Lock();
   mList.UpdateUI(false);
   mpFieldAtom1->SetValue(mpMolDihedralAngle->GetAtom1().GetName().c_str());
   mpFieldAtom2->SetValue(mpMolDihedralAngle->GetAtom2().GetName().c_str());
   mpFieldAtom3->SetValue(mpMolDihedralAngle->GetAtom3().GetName().c_str());
   if(lock) mMutex.Unlock();
   VFN_DEBUG_EXIT("WXMolDihedralAngle::UpdateUI()",5)
}

void WXMolDihedralAngle::OnChangeAtom(wxCommandEvent &event)
{
   VFN_DEBUG_ENTRY("WXMolDihedralAngle::OnChangeAtom()",6)
   WXCrystValidateAllUserInput();
   int choice;
   MolAtom *const at=WXDialogChooseFromVector(mpMolDihedralAngle->GetMolecule().GetAtomList(),
                               (wxWindow*)this,"Choose a new Atom",choice);
   if(0==at) return;
   if((long)ID_MOLDIHEDRALANGLE_ATOM1==event.GetId())
   {
      if(  (at==&(mpMolDihedralAngle->GetAtom2()))
         ||(at==&(mpMolDihedralAngle->GetAtom3()))
         ||(at==&(mpMolDihedralAngle->GetAtom4())) )
      {
         wxMessageDialog dumbUser(this,_T("The four atoms must be different !"),
                                  _T("Whooops"),wxOK|wxICON_EXCLAMATION);
         dumbUser.ShowModal();
         return;
      }
      mpMolDihedralAngle->SetAtom1(*at);
   }
   if((long)ID_MOLDIHEDRALANGLE_ATOM2==event.GetId())
   {
      if(  (at==&(mpMolDihedralAngle->GetAtom1()))
         ||(at==&(mpMolDihedralAngle->GetAtom3()))
         ||(at==&(mpMolDihedralAngle->GetAtom4())) )
      {
         wxMessageDialog dumbUser(this,_T("The four atoms must be different !"),
                                  _T("Whooops"),wxOK|wxICON_EXCLAMATION);
         dumbUser.ShowModal();
         return;
      }
      mpMolDihedralAngle->SetAtom2(*at);
   }
   if((long)ID_MOLDIHEDRALANGLE_ATOM3==event.GetId())
   {
      if(  (at==&(mpMolDihedralAngle->GetAtom1()))
         ||(at==&(mpMolDihedralAngle->GetAtom2()))
         ||(at==&(mpMolDihedralAngle->GetAtom4())) )
      {
         wxMessageDialog dumbUser(this,_T("The four atoms must be different !"),
                                  _T("Whooops"),wxOK|wxICON_EXCLAMATION);
         dumbUser.ShowModal();
         return;
      }
      mpMolDihedralAngle->SetAtom3(*at);
   }
   if((long)ID_MOLDIHEDRALANGLE_ATOM4==event.GetId())
   {
      if(  (at==&(mpMolDihedralAngle->GetAtom1()))
         ||(at==&(mpMolDihedralAngle->GetAtom2()))
         ||(at==&(mpMolDihedralAngle->GetAtom3())) )
      {
         wxMessageDialog dumbUser(this,_T("The four atoms must be different !"),
                                  _T("Whooops"),wxOK|wxICON_EXCLAMATION);
         dumbUser.ShowModal();
         return;
      }
      mpMolDihedralAngle->SetAtom4(*at);
   }

   this->CrystUpdate(true);
   this->UpdateUI(true);
   VFN_DEBUG_EXIT("WXMolDihedralAngle::OnChangeScattPow()",6)
}

////////////////////////////////////////////////////////////////////////
//
//    WXMolecule Grid objects
//
////////////////////////////////////////////////////////////////////////
WXMolecule::CellAtom::CellAtom():
mpAtom(0),mName(""),mpScatteringPower(0),mX(0),mY(0),mZ(0),mNeedUpdateUI(true)
{}

WXMolecule::CellBond::CellBond():
mpBond(0),mAtom1(""),mAtom2(""),
mLength(0),mLength0(0),mSigma(0),mDelta(0),mNeedUpdateUI(true)
{}

WXMolecule::CellBondAngle::CellBondAngle():
mpBondAngle(0),mAtom1(""),mAtom2(""),mAtom3(""),
mAngle(0),mAngle0(0),mSigma(0),mDelta(0),mNeedUpdateUI(true)
{}

WXMolecule::CellDihedralAngle::CellDihedralAngle():
mpDihedralAngle(0),mAtom1(""),mAtom2(""),mAtom3(""),mAtom4(""),
mAngle(0),mAngle0(0),mSigma(0),mDelta(0),mNeedUpdateUI(true)
{}

WXMolecule::CellRigidGroup::CellRigidGroup():
mpGroup(0),mNeedUpdateUI(false)
{}


////////////////////////////////////////////////////////////////////////
//
//    WXMolecule
//
////////////////////////////////////////////////////////////////////////

WXCRYST_ID ID_MENU_OPTIMIZECONFORMATION;
WXCRYST_ID ID_MENU_SETLIMITS;
WXCRYST_ID ID_MOLECULE_MENU_FILE;
WXCRYST_ID ID_MOLECULE_MENU_FILE_2ZMATRIX;
WXCRYST_ID ID_MOLECULE_MENU_FILE_2ZMATRIXNAMED;
WXCRYST_ID ID_MOLECULE_MENU_PAR_MDTEST;
WXCRYST_ID ID_MOLECULE_MENU_FORMULA;
WXCRYST_ID ID_MOLECULE_MENU_FORMULA_OPTIMIZECONFORMATION;
WXCRYST_ID ID_MOLECULE_MENU_FORMULA_STATUS;
WXCRYST_ID ID_MOLECULE_MENU_EXPORT_RESTRAINTS;
WXCRYST_ID ID_MOLECULE_MENU_FORMULA_ADD_ATOM;
WXCRYST_ID ID_MOLECULE_MENU_FORMULA_ADD_BOND;
WXCRYST_ID ID_MOLECULE_MENU_FORMULA_ADD_ANGLE;
WXCRYST_ID ID_MOLECULE_MENU_FORMULA_ADD_DIHEDRAL;
WXCRYST_ID ID_MOLECULE_MENU_FORMULA_ADD_RIGID_GROUP;
WXCRYST_ID ID_MOLECULE_MENU_FORMULA_ADD_NONFLIP_ATOM;
WXCRYST_ID ID_MOLECULE_MENU_FORMULA_RIGIDIFY_WITH_DIHEDRALANGLES;
WXCRYST_ID ID_MOLECULE_MENU_FORMULA_TEST;
WXCRYST_ID ID_MOLECULE_MENU_FORMULA_REMOVE_ATOM;
WXCRYST_ID ID_MOLECULE_MENU_FORMULA_REMOVE_BOND;
WXCRYST_ID ID_MOLECULE_MENU_FORMULA_REMOVE_ANGLE;
WXCRYST_ID ID_MOLECULE_MENU_FORMULA_REMOVE_DIHEDRAL;
WXCRYST_ID ID_MOLECULE_MENU_FORMULA_REMOVE_RIGID_GROUP;
WXCRYST_ID ID_MOLECULE_MENU_FORMULA_REMOVE_NONFLIPATOM;
WXCRYST_ID ID_MOLECULE_MENU_FORMULA_SHOW_RESTRAINT;
WXCRYST_ID ID_MOLECULE_MENU_FORMULA_SET_DELTA_SIGMA;
WXCRYST_ID ID_MOLECULE_MENU_GEOMETRY;
WXCRYST_ID ID_MOLECULE_MENU_GEOMETRY_ROTATE_BOND;
WXCRYST_ID ID_MOLECULE_MENU_GEOMETRY_ROTATE_DIHED;

WXCRYST_ID ID_MOLECULE_CHANGE_CENTER_ATOM;


WXCRYST_ID ID_WINDOW_ATOM;
WXCRYST_ID ID_WINDOW_BONDLENGTH;
WXCRYST_ID ID_WINDOW_BONDANGLE;
WXCRYST_ID ID_WINDOW_DIHEDRALANGLE;
WXCRYST_ID ID_WINDOW_RIGIDGROUP;
WXCRYST_ID ID_WINDOW_NONFLIPATOM;

BEGIN_EVENT_TABLE(WXMolecule,wxWindow)
   EVT_BUTTON(ID_WXOBJ_COLLAPSE,                          WXCrystObj::OnToggleCollapse)
   EVT_MENU(ID_REFOBJ_MENU_PAR_FIXALL,                    WXRefinableObj::OnMenuFixAllPar)
   EVT_MENU(ID_REFOBJ_MENU_PAR_UNFIXALL,                  WXRefinableObj::OnMenuUnFixAllPar)
   EVT_MENU(ID_REFOBJ_MENU_PAR_RANDOMIZE,                 WXRefinableObj::OnMenuParRandomize)
   EVT_MENU(ID_MOLECULE_MENU_PAR_MDTEST,                  WXMolecule::OnMenuMDTest)
   EVT_MENU(ID_MOLECULE_MENU_FORMULA_OPTIMIZECONFORMATION,WXMolecule::OnMenuOptimizeConformation)
   EVT_MENU(ID_MOLECULE_MENU_FORMULA_STATUS,              WXMolecule::OnMenuPrintRestraintStatus)
   EVT_MENU(ID_MOLECULE_MENU_EXPORT_RESTRAINTS,           WXMolecule::OnMenuExportRestraints)
   EVT_MENU(ID_MOLECULE_MENU_FORMULA_ADD_ATOM,            WXMolecule::OnMenuAddAtom)
   EVT_MENU(ID_MOLECULE_MENU_FORMULA_ADD_BOND,            WXMolecule::OnMenuAddBond)
   EVT_MENU(ID_MOLECULE_MENU_FORMULA_ADD_ANGLE,           WXMolecule::OnMenuAddAngle)
   EVT_MENU(ID_MOLECULE_MENU_FORMULA_ADD_DIHEDRAL,        WXMolecule::OnMenuAddDihedralAngle)
   EVT_MENU(ID_MOLECULE_MENU_FORMULA_ADD_RIGID_GROUP,     WXMolecule::OnMenuAddRigidGroup)
   EVT_MENU(ID_MOLECULE_MENU_FORMULA_ADD_NONFLIP_ATOM,    WXMolecule::OnMenuAddNonFlipAtom)
   EVT_MENU(ID_MOLECULE_MENU_FORMULA_RIGIDIFY_WITH_DIHEDRALANGLES,WXMolecule::OnMenuRigidfyWithDihedralAngles)
   EVT_MENU(ID_MOLECULE_MENU_FORMULA_REMOVE_ATOM,         WXMolecule::OnMenuRemoveAtom)
   EVT_MENU(ID_MOLECULE_MENU_FORMULA_REMOVE_BOND,         WXMolecule::OnMenuRemoveBond)
   EVT_MENU(ID_MOLECULE_MENU_FORMULA_REMOVE_ANGLE,        WXMolecule::OnMenuRemoveAngle)
   EVT_MENU(ID_MOLECULE_MENU_FORMULA_REMOVE_DIHEDRAL,     WXMolecule::OnMenuRemoveDihedralAngle)
   EVT_MENU(ID_MOLECULE_MENU_FORMULA_REMOVE_RIGID_GROUP,  WXMolecule::OnMenuRemoveRigidGroup)
   EVT_MENU(ID_MOLECULE_MENU_FORMULA_REMOVE_NONFLIPATOM,  WXMolecule::OnMenuRemoveNonFlipAtom)
   EVT_MENU(ID_MOLECULE_MENU_FORMULA_TEST        ,        WXMolecule::OnMenuTest)
   EVT_MENU(ID_MENU_SETLIMITS,                            WXMolecule::OnMenuSetLimits)
   EVT_MENU(ID_MOLECULE_MENU_FORMULA_SHOW_RESTRAINT,      WXMolecule::OnMenuShowRestraintWindow)
   EVT_MENU(ID_MOLECULE_MENU_FORMULA_SET_DELTA_SIGMA     ,WXMolecule::OnMenuSetDeltaSigma)
   EVT_MENU(ID_MOLECULE_MENU_FILE_2ZMATRIX,               WXMolecule::OnMenuExport2ZMatrix)
   EVT_MENU(ID_MOLECULE_MENU_FILE_2ZMATRIXNAMED,          WXMolecule::OnMenuExport2ZMatrix)
   EVT_MENU(ID_MOLECULE_MENU_GEOMETRY_ROTATE_BOND,        WXMolecule::OnMenuRotate)
   EVT_MENU(ID_MOLECULE_MENU_GEOMETRY_ROTATE_DIHED,        WXMolecule::OnMenuRotate)
   EVT_GRID_CMD_CELL_CHANGE(ID_WINDOW_ATOM,               WXMolecule::OnEditGridAtom)
   EVT_GRID_CMD_CELL_CHANGE(ID_WINDOW_BONDLENGTH,         WXMolecule::OnEditGridBondLength)
   EVT_GRID_CMD_CELL_CHANGE(ID_WINDOW_BONDANGLE,          WXMolecule::OnEditGridBondAngle)
   EVT_GRID_CMD_CELL_CHANGE(ID_WINDOW_DIHEDRALANGLE,      WXMolecule::OnEditGridDihedralAngle)
   EVT_GRID_CMD_CELL_CHANGE(ID_WINDOW_RIGIDGROUP,         WXMolecule::OnEditGridRigidGroup)
   EVT_BUTTON(ID_MOLECULE_CHANGE_CENTER_ATOM,             WXMolecule::OnChangeCenterAtom)
END_EVENT_TABLE()

WXMolecule::WXMolecule(wxWindow *parent, Molecule *mol):
WXScatterer(parent,mol),mpMolecule(mol),
mpBondWin(0),mpAngleWin(0),mpDihedralAngleWin(0),mpRigidGroupWin(0),mpNonFlipAtomWin(0),mIsSelfUpdating(false)
{
   VFN_DEBUG_ENTRY("WXMolecule::WXMolecule():"<<mol->GetName(),6)
   //Menus
      mpMenuBar->AddMenu("File",ID_MOLECULE_MENU_FILE);
         mpMenuBar->AddMenuItem(ID_MOLECULE_MENU_FILE,ID_MOLECULE_MENU_FILE_2ZMATRIX,"Export to Fenske-Hall Z-Matrix");
         mpMenuBar->AddMenuItem(ID_MOLECULE_MENU_FILE,ID_MOLECULE_MENU_FILE_2ZMATRIXNAMED,"Export to Z-Matrix with atom names");
      mpMenuBar->AddMenu("Parameters",ID_REFOBJ_MENU_PAR);
         mpMenuBar->AddMenuItem(ID_REFOBJ_MENU_PAR,ID_REFOBJ_MENU_PAR_FIXALL,"Fix all");
         mpMenuBar->AddMenuItem(ID_REFOBJ_MENU_PAR,ID_REFOBJ_MENU_PAR_UNFIXALL,"Unfix all");
      mpMenuBar->AddMenuItem(ID_REFOBJ_MENU_PAR,ID_REFOBJ_MENU_PAR_RANDOMIZE,
                          "Randomize Configuration");
      mpMenuBar->AddMenuItem(ID_REFOBJ_MENU_PAR,ID_MOLECULE_MENU_PAR_MDTEST,
                          "Test Molecular Dynamics moves for 30s");
      mpMenuBar->AddMenu("Formula && Restraints",ID_MOLECULE_MENU_FORMULA);
         mpMenuBar->AddMenuItem(ID_MOLECULE_MENU_FORMULA,ID_MOLECULE_MENU_FORMULA_OPTIMIZECONFORMATION,
                                "Optimize Starting Conformation");
         mpMenuBar->AddMenuItem(ID_MOLECULE_MENU_FORMULA,ID_MOLECULE_MENU_FORMULA_SET_DELTA_SIGMA,
                                "Set Restraints delta && sigma for all bonds && angles");
         mpMenuBar->AddMenuItem(ID_MOLECULE_MENU_FORMULA,ID_MOLECULE_MENU_FORMULA_STATUS,
                                "Print Detailed Restraint Values");
         mpMenuBar->AddMenuItem(ID_MOLECULE_MENU_FORMULA,ID_MOLECULE_MENU_EXPORT_RESTRAINTS,
                                "Export Restraints in CVS File format");
         mpMenuBar->GetMenu(ID_MOLECULE_MENU_FORMULA).AppendSeparator();
         mpMenuBar->AddMenuItem(ID_MOLECULE_MENU_FORMULA,ID_MOLECULE_MENU_FORMULA_ADD_ATOM,
                                "Add an Atom");
         mpMenuBar->AddMenuItem(ID_MOLECULE_MENU_FORMULA,ID_MOLECULE_MENU_FORMULA_ADD_BOND,
                                "Add Bond Restraint");
         mpMenuBar->AddMenuItem(ID_MOLECULE_MENU_FORMULA,ID_MOLECULE_MENU_FORMULA_ADD_ANGLE,
                                "Add Bond Angle Restraint");
         mpMenuBar->AddMenuItem(ID_MOLECULE_MENU_FORMULA,ID_MOLECULE_MENU_FORMULA_ADD_DIHEDRAL,
                                "Add Dihedral Angle Restraint");
         mpMenuBar->AddMenuItem(ID_MOLECULE_MENU_FORMULA,ID_MOLECULE_MENU_FORMULA_ADD_RIGID_GROUP,
                                "Add Rigid Group");
         mpMenuBar->AddMenuItem(ID_MOLECULE_MENU_FORMULA,ID_MOLECULE_MENU_FORMULA_ADD_NONFLIP_ATOM,
                                "Add Non-Flip (optically active) Atom");
         mpMenuBar->AddMenuItem(ID_MOLECULE_MENU_FORMULA,ID_MOLECULE_MENU_FORMULA_RIGIDIFY_WITH_DIHEDRALANGLES,
                                "Rigidify with Dihedral Angles");
         mpMenuBar->GetMenu(ID_MOLECULE_MENU_FORMULA).AppendSeparator();
         mpMenuBar->AddMenuItem(ID_MOLECULE_MENU_FORMULA,ID_MOLECULE_MENU_FORMULA_REMOVE_ATOM,
                                "Remove an Atom");
         mpMenuBar->AddMenuItem(ID_MOLECULE_MENU_FORMULA,ID_MOLECULE_MENU_FORMULA_REMOVE_BOND,
                                "Remove a Bond Restraint");
         mpMenuBar->AddMenuItem(ID_MOLECULE_MENU_FORMULA,ID_MOLECULE_MENU_FORMULA_REMOVE_ANGLE,
                                "Remove a Bond Angle Restraint");
         mpMenuBar->AddMenuItem(ID_MOLECULE_MENU_FORMULA,ID_MOLECULE_MENU_FORMULA_REMOVE_DIHEDRAL,
                                "Remove a Dihedral Angle Restraint");
         mpMenuBar->AddMenuItem(ID_MOLECULE_MENU_FORMULA,ID_MOLECULE_MENU_FORMULA_REMOVE_RIGID_GROUP,
                                "Remove Rigid Group");
         mpMenuBar->AddMenuItem(ID_MOLECULE_MENU_FORMULA,ID_MOLECULE_MENU_FORMULA_REMOVE_NONFLIPATOM,
                                "Remove Non-Flip (optically active) Atom");

         mpMenuBar->GetMenu(ID_MOLECULE_MENU_FORMULA).AppendSeparator();
         mpMenuBar->AddMenuItem(ID_MOLECULE_MENU_FORMULA,ID_MOLECULE_MENU_FORMULA_SHOW_RESTRAINT,
                                "Show Restraint Window");
         //#ifdef __DEBUG__
         mpMenuBar->AddMenuItem(ID_MOLECULE_MENU_FORMULA,ID_MOLECULE_MENU_FORMULA_TEST,"Debug Test");
         //#endif
      mpMenuBar->AddMenu("Manipulate Geometry",ID_MOLECULE_MENU_GEOMETRY);
         mpMenuBar->AddMenuItem(ID_MOLECULE_MENU_GEOMETRY,ID_MOLECULE_MENU_GEOMETRY_ROTATE_BOND,
                                "Rotate around bond");
         mpMenuBar->AddMenuItem(ID_MOLECULE_MENU_GEOMETRY,ID_MOLECULE_MENU_GEOMETRY_ROTATE_DIHED,
                                "Set dihedral angle");
      //mpSizer->SetItemMinSize(mpMenuBar,
      //                        mpMenuBar->GetSize().GetWidth(),
      //                        mpMenuBar->GetSize().GetHeight());
      WXFieldPar<REAL> *pLLKScale= new WXFieldPar<REAL>(this,"Log(likelihood) scale",-1,&(mpMolecule->mLogLikelihoodScale));
      mList.Add(pLLKScale);
      mpSizer->Add(pLLKScale,0,wxALIGN_LEFT);
      pLLKScale->SetToolTip(_T("The log(likelihood) of the molecule, i.e. the sum\n")
                            _T("of the restraints costs for all bonds and angles \n")
                            _T("are multiplied by this scale.\n\n")
                            _T("The default value is 1.\n\n")
                            _T("Multiplying this value by 4 is equivalent to dividing the\n")
                            _T("sigma of all bonds and angles by a factor 2.\n")
                            _T("This can be increased if the molecule presents too\n")
                            _T("much distortion.\n\n")
                            _T("Note that a too large value (e.g.>100) will decrease the\n")
                            _T("convergence speed of the algorithm, as distortion of the\n")
                            _T("structure is necessary for an efficient minimisation."));

   // Molecular Dynamics move parameters
      wxBoxSizer* pMDSizer=new wxBoxSizer(wxHORIZONTAL);
      WXFieldPar<REAL> *pWXFieldMDEnergy=
         new WXFieldPar<REAL>(this,"MD moves Energy",-1,&(mpMolecule->mMDMoveEnergy));
      WXFieldPar<REAL> *pWXFieldMDFrequency=
         new WXFieldPar<REAL>(this,"MD moves frequency",-1,&(mpMolecule->mMDMoveFreq));
      pMDSizer->Add(pWXFieldMDFrequency);
      pMDSizer->Add(pWXFieldMDEnergy);
      mpSizer->Add(pMDSizer,0,wxALIGN_LEFT);
      mList.Add(pWXFieldMDEnergy);
      mList.Add(pWXFieldMDFrequency);

      pWXFieldMDFrequency->SetToolTip(_T("Frequency of Molecular Dynamics Moves (0.0-1.0)\n\n")
                                      _T("MD moves are used to alter the conformation of atom groups \n")
                                      _T("which cannot be changed by simple bond distance\n")
                                      _T("or bond angle changes.\n\n")
                                      _T("MD moves are CPU intensive, so a slow value (0.05) is recommended\n")
                                      _T("Larger frequencies can be used for constrained molecules,\n")
                                      _T("e.g. with large, flexible cycles."));
      pWXFieldMDEnergy->SetToolTip(_T("Energy of Molecule for Molecular Dynamics Moves\n\n")
                                   _T("Standard amplitude=40. \n")
                                   _T("Small amplitude=10. (small distortion of the Molecule)\n")
                                   _T("Large amplitude=400. (large distortion of the Molecule)"));
   //Center atom
      mpFieldCenterAtom=new WXFieldChoice(this,ID_MOLECULE_CHANGE_CENTER_ATOM,"Center Atom:",240);
      if(mpMolecule->GetCenterAtom()!=0)
         mpFieldCenterAtom->SetValue(mpMolecule->GetCenterAtom()->GetName());
      else mpFieldCenterAtom->SetValue("Click to choose an atom !");
      mpSizer->Add(mpFieldCenterAtom,0,wxALIGN_LEFT);
      mList.Add(mpFieldCenterAtom);
   // Atom list
      wxGridCellAttr* cellAttrName = new wxGridCellAttr;
      cellAttrName->SetRenderer(new wxGridCellStringRenderer);
      cellAttrName->SetEditor(new wxGridCellTextEditor);
      wxGridCellAttr* cellAttrFloat = new wxGridCellAttr;
      cellAttrFloat->SetRenderer(new wxGridCellFloatRenderer(6,3));
      cellAttrFloat->SetEditor(new wxGridCellFloatEditor(6,3));

      mpAtomWin= new WXMolScrolledWindow(this,this,ID_WINDOW_ATOM);
      //mpAtomWin->SetSize(800,300);
      mpAtomWin->EnableScrolling(true,true);
      //mpAtomWin->SetSizeHints(-1,300,-1,300);
      mpAtomWin->CreateGrid(0,6);
      mpAtomWin->SetRowLabelSize(40);
      mpAtomWin->SetDefaultColSize(60);
      mpAtomWin->SetColAttr(0,cellAttrName);
      mpAtomWin->SetColAttr(1,cellAttrName->Clone());
      mpAtomWin->SetColAttr(2,cellAttrFloat);
      mpAtomWin->SetColAttr(3,cellAttrFloat->Clone());
      mpAtomWin->SetColAttr(4,cellAttrFloat->Clone());
      mpAtomWin->SetColAttr(5,cellAttrFloat->Clone());
      mpAtomWin->SetColLabelValue(0,_T("Name"));
      mpAtomWin->SetColLabelValue(1,_T("Type"));
      mpAtomWin->SetColLabelValue(2,_T("X"));
      mpAtomWin->SetColLabelValue(3,_T("Y"));
      mpAtomWin->SetColLabelValue(4,_T("Z"));
      mpAtomWin->SetColLabelValue(5,_T("Occup"));
      mpAtomWin->AutoSizeRows();
      mpSizer->Add(mpAtomWin,0,wxALIGN_LEFT);
   this->CrystUpdate(true);
   VFN_DEBUG_EXIT("WXMolecule::WXMolecule():"<<mol->GetName(),6)
}

WXMolecule::~WXMolecule()
{
   VFN_DEBUG_ENTRY("WXMolecule::~WXMolecule()",10)
   if(0!=mpBondWin) mpBondWin->GetParent()->Destroy();
   VFN_DEBUG_EXIT("WXMolecule::~WXMolecule()",10)
}

void WXMolecule::OnMenuOptimizeConformation(wxCommandEvent & WXUNUSED(event))
{
   VFN_DEBUG_ENTRY("WXMolecule::OnMenuOptimizeConformation()",5)
   WXCrystValidateAllUserInput();
   mpMolecule->OptimizeConformation(20000,mpMolecule->GetAtomList().size());
   mpMolecule->OptimizeConformationSteepestDescent(0.01,100);
   mpMolecule->RestraintStatus(cout);

   LSQNumObj lsq;
   lsq.SetRefinedObj(*mpMolecule);
   lsq.PrepareRefParList(true);
   lsq.SetParIsUsed(gpRefParTypeObjCryst,false);
   lsq.SetParIsUsed(gpRefParTypeScattConform,true);
   try {lsq.Refine(10,true,false);}
   catch(const ObjCrystException &except){}
   mpMolecule->GetCrystal().UpdateDisplay();
   mpMolecule->RestraintStatus(cout);

   mpMolecule->GetCrystal().UpdateDisplay();

   VFN_DEBUG_EXIT("WXMolecule::OnMenuOptimizeConformation()",5)
}
void WXMolecule::OnMenuExportRestraints(wxCommandEvent & WXUNUSED(event))
{
   VFN_DEBUG_ENTRY("WXMolecule::OnMenuExportRestraints()",5)
   WXCrystValidateAllUserInput();

   stringstream ss;
   mpMolecule->RestraintExport(ss);

   wxFileDialog open(this,_T("Choose File to save restraints:"),
                        _T(""),_T(""),_T("*.csv"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
   if(open.ShowModal() != wxID_OK) return;
   wxString name=open.GetPath();
   if(name.substr(name.size()-4,4)!=_T(".csv"))
   {
       cout<<name<<" -> "<<name+_T(".csv")<<endl;
       name=name+_T(".csv");
   }

   wxFileOutputStream ostream(name.c_str());
   ostream.Write(ss.str().c_str(),ss.str().size());

   VFN_DEBUG_EXIT("WXMolecule::OnMenuExportRestraints()",5)
}
void WXMolecule::OnMenuPrintRestraintStatus(wxCommandEvent & WXUNUSED(event))
{
   VFN_DEBUG_ENTRY("WXMolecule::OnMenuPrintRestraintStatus()",5)
   WXCrystValidateAllUserInput();
   mpMolecule->RestraintStatus(cout);
   VFN_DEBUG_EXIT("WXMolecule::OnMenuPrintRestraintStatus()",5)
}

void WXMolecule::OnMenuAddAtom(wxCommandEvent & WXUNUSED(event))
{
   VFN_DEBUG_ENTRY("WXMolecule::OnMenuAddAtom()",6)
   WXCrystValidateAllUserInput();
   int choice;
   const ScatteringPower *scatt=WXDialogChooseFromRegistry(
               mpMolecule->GetCrystal().GetScatteringPowerRegistry(),
               (wxWindow*)this,"Choose a new Scattering Power",choice);
   if(0==scatt) return;
   long num=mpMolecule->GetAtomList().size();
   if(num>0)
   {
      wxString lastAtom=wxString::FromAscii(mpMolecule->GetAtom(num-1).GetName().c_str());
      for(;;)
      {
         if(lastAtom.size()==0) break;
         if(lastAtom.IsNumber())
         {
            lastAtom.ToLong(&num);
            break;
         }
         lastAtom.erase(0,1);
      }
   }
   stringstream st;
   st<<num+1;
   mpMolecule->AddAtom(0.,0.,0.,scatt,scatt->GetName()+st.str());
   VFN_DEBUG_EXIT("WXMolecule::OnMenuAddAtom()",6)
}

void WXMolecule::OnMenuAddBond(wxCommandEvent & WXUNUSED(event))
{
   VFN_DEBUG_ENTRY("WXMolecule::OnMenuAddBond()",6)
   WXCrystValidateAllUserInput();
   int choice;
   vector<MolAtom*> v=mpMolecule->GetAtomList();
   MolAtom *at1=WXDialogChooseFromVector(v,
                               (wxWindow*)this,"Choose the first Atom",choice);
   if(0==at1) return;
   MolAtom *at2=WXDialogChooseFromVector(v,
                               (wxWindow*)this,"Choose the second Atom",choice);
   if(0==at2) return;

   if(at1==at2)
   {
      wxMessageDialog dumbUser(this,_T("The two atoms must be different !"),
                               _T("Whooops"),wxOK|wxICON_EXCLAMATION);
      dumbUser.ShowModal();
      return;
   }
   static double d=1.5;//static so that last entered value will be used next time
   wxString s;
   s.Printf(_T("%6.3f"),d);
   // if((at1->IsDummy())||(at2->IsDummy())) d=1.5;
   // else d= (at1->GetScatteringPower().GetRadius()+at2->GetScatteringPower().GetRadius())*0.9;
   string mes="Enter bond distance (Angstroems) for "+at1->GetName()+"-"+at2->GetName();
   wxTextEntryDialog dialog(this,wxString::FromAscii(mes.c_str()),
                           _T("Bond distance"),s,wxOK | wxCANCEL);
   if(wxID_OK!=dialog.ShowModal())
   {
      VFN_DEBUG_EXIT("WXMolecule::OnMenuAddBond():Canceled",6)
      return;
   }
   dialog.GetValue().ToDouble(&d);

   mpMolecule->AddBond(*at1,*at2,d,.01,.02,1.);
   VFN_DEBUG_EXIT("WXMolecule::OnMenuAddBond()",6)
}

void WXMolecule::OnMenuAddAngle(wxCommandEvent & WXUNUSED(event))
{
   VFN_DEBUG_ENTRY("WXMolecule::OnMenuAddBond()",6)
   WXCrystValidateAllUserInput();
   int choice;
   vector<MolAtom*> v=mpMolecule->GetAtomList();
   MolAtom *at1=WXDialogChooseFromVector(v,
                               (wxWindow*)this,"Choose the first Atom",choice);
   if(0==at1) return;
   MolAtom *at2=WXDialogChooseFromVector(v,
                               (wxWindow*)this,"Choose the second Atom",choice);
   if(0==at2) return;

   MolAtom *at3=WXDialogChooseFromVector(v,
                               (wxWindow*)this,"Choose the third Atom",choice);
   if(0==at2) return;

   if( (at1==at2) || (at1==at3) ||(at2==at3))
   {
      wxMessageDialog dumbUser(this,_T("The three atoms must be different !"),
                               _T("Whooops"),wxOK|wxICON_EXCLAMATION);
      dumbUser.ShowModal();
      return;
   }

   static double a=109.5;//static so that last entered value will be used next time
   wxString s;
   s.Printf(_T("%6.2f"),a);
   string mes="Enter bond angle (degrees) for "+at1->GetName()
                                           +"-"+at2->GetName()
                                           +"-"+at3->GetName();
   wxTextEntryDialog dialog(this,wxString::FromAscii(mes.c_str()),
                           _T("Bond angle"),s,wxOK | wxCANCEL);
   if(wxID_OK!=dialog.ShowModal())
   {
      VFN_DEBUG_EXIT("WXMolecule::OnMenuAddAngle():Canceled",6)
      return;
   }
   dialog.GetValue().ToDouble(&a);

   mpMolecule->AddBondAngle(*at1,*at2,*at3,a*DEG2RAD,.01,0.02);
   VFN_DEBUG_EXIT("WXMolecule::OnMenuAddBond()",6)
}

void WXMolecule::OnMenuAddDihedralAngle(wxCommandEvent & WXUNUSED(event))
{
   VFN_DEBUG_ENTRY("WXMolecule::OnMenuAddDihedralAngle()",6)
   WXCrystValidateAllUserInput();
   int choice;
   vector<MolAtom*> v=mpMolecule->GetAtomList();
   MolAtom *at1=WXDialogChooseFromVector(v,
                               (wxWindow*)this,"Choose the first Atom",choice);
   if(0==at1) return;
   MolAtom *at2=WXDialogChooseFromVector(v,
                               (wxWindow*)this,"Choose the second Atom",choice);
   if(0==at2) return;

   MolAtom *at3=WXDialogChooseFromVector(v,
                               (wxWindow*)this,"Choose the third Atom",choice);
   if(0==at3) return;

   MolAtom *at4=WXDialogChooseFromVector(v,
                               (wxWindow*)this,"Choose the fourth Atom",choice);
   if(0==at4) return;

   if( (at1==at2) || (at1==at3) || (at1==at4) || (at2==at3) || (at2==at4) || (at3==at4))
   {
      wxMessageDialog dumbUser(this,_T("The atoms must be different !"),
                               _T("Whooops"),wxOK|wxICON_EXCLAMATION);
      dumbUser.ShowModal();
      return;
   }

   static double a=180;
   wxString s;
   s.Printf(_T("%6.2f"),a);
   string mes="Enter dihedral angle (degrees) for "+at1->GetName()
                                               +"-"+at2->GetName()
                                               +"-"+at3->GetName()
                                               +"-"+at4->GetName();
   wxTextEntryDialog dialog(this,_T("Enter dihedral angle (degrees)"),
                           _T("Bond angle"),s,wxOK | wxCANCEL);
   if(wxID_OK!=dialog.ShowModal())
   {
      VFN_DEBUG_EXIT("WXMolecule::OnMenuAddDihedralAngle():Canceled",6)
      return;
   }
   dialog.GetValue().ToDouble(&a);

   mpMolecule->AddDihedralAngle(*at1,*at2,*at3,*at4,a*DEG2RAD,.01,.02);
   VFN_DEBUG_EXIT("WXMolecule::OnMenuAddDihedralAngle()",6)
}

void WXMolecule::OnMenuAddRigidGroup(wxCommandEvent & WXUNUSED(event))
{
   VFN_DEBUG_ENTRY("WXMolecule::OnMenuAddRigidGroup()",6)
   WXCrystValidateAllUserInput();
   list<MolAtom*> l=WXDialogChooseMultipleFromVector(mpMolecule->GetAtomList(),this,
                                                     "Choose atoms in the rigid group");
   RigidGroup s;
   for(list<MolAtom*>::const_iterator pos=l.begin();pos!=l.end();++pos) s.insert(*pos);
   mpMolecule->AddRigidGroup(s);
   VFN_DEBUG_EXIT("WXMolecule::OnMenuAddRigidGroup()",6)
}

void WXMolecule::OnMenuAddNonFlipAtom(wxCommandEvent & WXUNUSED(event))
{
   VFN_DEBUG_ENTRY("WXMolecule::OnMenuAddNonFlipAtom()",6)
   WXCrystValidateAllUserInput();
   int choice;
   vector<MolAtom*> v;
   for(vector<MolAtom*>::const_iterator pos=mpMolecule->GetAtomList().begin();pos!=mpMolecule->GetAtomList().end();++pos)
   {
      if((*pos)->IsNonFlipAtom()==false) v.push_back(*pos);
   }

   MolAtom *at1=WXDialogChooseFromVector(v,
                               (wxWindow*)this,"Choose the Optically active Atom",choice);

   if(0==at1) {
      wxMessageDialog dumbUser(this,_T("No atom selected !"),
                               _T("Whooops"),wxOK|wxICON_EXCLAMATION);
      dumbUser.ShowModal();
      return;
   }

   at1->SetNonFlipAtom(true);
   this->CrystUpdate(true);
   VFN_DEBUG_EXIT("WXMolecule::OnMenuAddNonFlipAtom()",6)
}

void WXMolecule::OnMenuRemoveAtom(wxCommandEvent & WXUNUSED(event))
{
   VFN_DEBUG_ENTRY("WXMolecule::OnMenuRemoveAtom()",6)
   vector<MolAtom*> v=mpMolecule->GetAtomList();
   list<MolAtom*> vAt=WXDialogChooseMultipleFromVector(v,(wxWindow*)this,
                                                       "Choose the Atom(s) to be removed");
   if(0==vAt.size()) return;
   for(list<MolAtom*>::iterator pos=vAt.begin();pos!=vAt.end();++pos) mpMolecule->RemoveAtom(**pos);
   this->CrystUpdate(true);
   VFN_DEBUG_EXIT("WXMolecule::OnMenuRemoveAtom()",6)
}

void WXMolecule::OnMenuRemoveBond(wxCommandEvent & WXUNUSED(event))
{
   VFN_DEBUG_ENTRY("WXMolecule::OnMenuRemoveBond()",6)
   vector<MolBond*> v=mpMolecule->GetBondList();
   list<MolBond*> vBond=WXDialogChooseMultipleFromVector(v,(wxWindow*)this,
                                                         "Choose the Bond(s) to be removed");
   if(0==vBond.size()) return;
   const int answer =wxMessageBox
                  (_T("Remove Bond and Dihedral Angles involving the deleted Bond(s) (if any) ?"),
                   _T("Delete related Restraints ?"),wxYES_NO, this);
   for(list<MolBond*>::iterator pos=vBond.begin();pos!=vBond.end();++pos)
   {
      if(answer==wxYES)
      {
         const MolAtom *pAtom1= &((*pos)->GetAtom1());
         const MolAtom *pAtom2= &((*pos)->GetAtom2());
         for(vector<MolBondAngle*>::iterator posb=mpMolecule->GetBondAngleList().begin();
             posb!=mpMolecule->GetBondAngleList().end();)
         {
            if(  ( (pAtom1==&((*posb)->GetAtom1())) && (pAtom2==&((*posb)->GetAtom2())) )
               ||( (pAtom1==&((*posb)->GetAtom2())) && (pAtom2==&((*posb)->GetAtom1())) )
               ||( (pAtom1==&((*posb)->GetAtom2())) && (pAtom2==&((*posb)->GetAtom3())) )
               ||( (pAtom1==&((*posb)->GetAtom3())) && (pAtom2==&((*posb)->GetAtom2())) ))
               posb=mpMolecule->RemoveBondAngle(**posb);
            else ++posb;
         }
         for(vector<MolDihedralAngle*>::iterator posb=mpMolecule->GetDihedralAngleList().begin();
             posb!=mpMolecule->GetDihedralAngleList().end();)
         {
            if(  ( (pAtom1==&((*posb)->GetAtom1())) && (pAtom2==&((*posb)->GetAtom2())) )
               ||( (pAtom1==&((*posb)->GetAtom2())) && (pAtom2==&((*posb)->GetAtom1())) )
               ||( (pAtom1==&((*posb)->GetAtom2())) && (pAtom2==&((*posb)->GetAtom3())) )
               ||( (pAtom1==&((*posb)->GetAtom3())) && (pAtom2==&((*posb)->GetAtom2())) )
               ||( (pAtom1==&((*posb)->GetAtom3())) && (pAtom2==&((*posb)->GetAtom4())) )
               ||( (pAtom1==&((*posb)->GetAtom4())) && (pAtom2==&((*posb)->GetAtom3())) ))
               posb=mpMolecule->RemoveDihedralAngle(**posb);
            else ++posb;
         }
      }
      mpMolecule->RemoveBond(**pos);
   }
   this->CrystUpdate(true);
   VFN_DEBUG_EXIT("WXMolecule::OnMenuRemoveBond()",6)
}

void WXMolecule::OnMenuRemoveAngle(wxCommandEvent & WXUNUSED(event))
{
   VFN_DEBUG_ENTRY("WXMolecule::OnMenuRemoveAngle()",6)
   vector<MolBondAngle*> v=mpMolecule->GetBondAngleList();
   list<MolBondAngle*> vAngle=WXDialogChooseMultipleFromVector(v,(wxWindow*)this,
                                                              "Choose the Bond Angle(s) to be removed");
   if(0==vAngle.size()) return;
   for(list<MolBondAngle*>::iterator pos=vAngle.begin();pos!=vAngle.end();++pos)
      mpMolecule->RemoveBondAngle(**pos);
   this->CrystUpdate(true);
   VFN_DEBUG_EXIT("WXMolecule::OnMenuRemoveAngle()",6)
}

void WXMolecule::OnMenuRemoveDihedralAngle(wxCommandEvent & WXUNUSED(event))
{
   VFN_DEBUG_ENTRY("WXMolecule::OnMenuRemoveDihedralAngle()",6)
   vector<MolDihedralAngle*> v=mpMolecule->GetDihedralAngleList();
   list<MolDihedralAngle*> vAngle=WXDialogChooseMultipleFromVector(v,(wxWindow*)this,
                                     "Choose the Dihedral Angle(s) to be removed");
   if(0==vAngle.size()) return;
   for(list<MolDihedralAngle*>::iterator pos=vAngle.begin();pos!=vAngle.end();++pos)
      mpMolecule->RemoveDihedralAngle(**pos);
   this->CrystUpdate(true);
   VFN_DEBUG_EXIT("WXMolecule::OnMenuRemoveDihedralAngle()",6)
}

void WXMolecule::OnMenuRemoveNonFlipAtom(wxCommandEvent & WXUNUSED(event))
{
    VFN_DEBUG_ENTRY("WXMolecule::OnMenuRemoveNonFlipAtom()",6)
   int choice;
   vector<MolAtom*> v;
   for(vector<MolAtom*>::const_iterator pos=mpMolecule->GetAtomList().begin();pos!=mpMolecule->GetAtomList().end();++pos)
   {
      if((*pos)->IsNonFlipAtom()) v.push_back(*pos);
   }

   if(v.size()==0) {
      wxMessageDialog dumbUser(this,_T("The list of optically active atoms is empty !"),
                               _T("Whooops"),wxOK|wxICON_EXCLAMATION);
      dumbUser.ShowModal();
      return;
   }
   MolAtom *at1=WXDialogChooseFromVector(v, (wxWindow*)this,"Choose the Optically active Atom to be removed",choice);
   if(0==at1) {
      wxMessageDialog dumbUser(this,_T("No atom selected !"),
                               _T("Whooops"),wxOK|wxICON_EXCLAMATION);
      dumbUser.ShowModal();
      return;
   }

   at1->SetNonFlipAtom(false);
   this->CrystUpdate(true);
   VFN_DEBUG_EXIT("WXMolecule::OnMenuRemoveNonFlipAtom()",6)
}

void WXMolecule::OnMenuRemoveRigidGroup(wxCommandEvent & WXUNUSED(event))
{
   VFN_DEBUG_ENTRY("WXMolecule::OnMenuRemoveRigidGroup()",6)
   WXCrystValidateAllUserInput();
   vector<RigidGroup*> v=mpMolecule->GetRigidGroupList();
   list<RigidGroup*> vGroup=WXDialogChooseMultipleFromVector(v,(wxWindow*)this,
                                     "Choose the Rigid Group(s) to be removed");
   if(0==vGroup.size()) return;
   for(list<RigidGroup*>::iterator pos=vGroup.begin();pos!=vGroup.end();++pos)
      mpMolecule->RemoveRigidGroup(**pos);
   this->CrystUpdate(true);
   VFN_DEBUG_EXIT("WXMolecule::OnMenuRemoveRigidGroup()",6)
}
/*
void WXMolecule::OnMenuRemoveFlipGroup(wxCommandEvent & WXUNUSED(event))
{
   VFN_DEBUG_ENTRY("WXMolecule::OnMenuRemoveFlipGroup()",6)
   WXCrystValidateAllUserInput();
   mpMolecule->BuildFlipGroup();
   list<Molecule::FlipGroup> v = mpMolecule->GetFlipGroupList();

   wxString *choices = new wxString[v.size()];
   int i=0;
   for(list<Molecule::FlipGroup>::iterator pos=v.begin();pos!=v.end();++pos) {
       if(pos->mvRotatedChainList.begin()->first==pos->mpAtom0) {
           choices[i] = pos->mpAtom0->GetName();
           choices[i] += ": ";
           choices[i] += pos->mpAtom1->GetName();
           choices[i] += " <-> ";
           choices[i] += pos->mpAtom2->GetName();
       } else {
           choices[i] = pos->mpAtom0->GetName();
           for(list<pair<const MolAtom *,set<MolAtom*> > >::const_iterator chain=pos->mvRotatedChainList.begin(); chain!=pos->mvRotatedChainList.end();++chain) {
               choices[i] += chain->first->GetName();
               choices[i] += " <-> ";
           }
           choices[i].Remove(choices[i].size()-5, choices[i].size());
       }
       i++;
   }

   wxMultiChoiceDialog_ListBox dialog ((wxWindow*)this, "Choose the Flip Group(s) to be removed",_T("Choose"),v.size(),choices);
   dialog.ShowModal();
   wxArrayInt choice=dialog.GetSelections();

   delete[] choices;

   this->CrystUpdate(true);
   VFN_DEBUG_EXIT("WXMolecule::OnMenuRemoveFlipGroup()",6)
}
*/
void WXMolecule::OnEditGridAtom(wxGridEvent &e)
{
   if(mIsSelfUpdating) return;
   VFN_DEBUG_ENTRY("WXMolecule::OnEditGridAtom():"<<e.GetRow()<<","<<e.GetCol(),10)
   const long r=e.GetRow();
   const long c=e.GetCol();
   if(c==0)
   {
      wxString s=mpAtomWin->GetCellValue(r,c);
      if(s!=_T(""))
         mpMolecule->GetAtomList()[r]->SetName(string(s.ToAscii()));
   }
   if(c==1)
   {
      wxString s=mpAtomWin->GetCellValue(r,c);
      if(s!=_T(""))
      {
         try
         {
            long p=mpMolecule->GetCrystal().GetScatteringPowerRegistry().Find(string(s.ToAscii()));
            if(p>=0) mpMolecule->GetAtomList()[r]->SetScatteringPower(
               mpMolecule->GetCrystal().GetScatteringPowerRegistry().GetObj(p));
         }
         catch(ObjCrystException){};
      }
   }
   if(c==2)
   {
      wxString s=mpAtomWin->GetCellValue(r,c);
      if(s!=_T(""))
      {
         double d;
         if(s.ToDouble(&d))
            mpMolecule->GetAtomList()[r]->SetX(d);
      }
   }
   if(c==3)
   {
      wxString s=mpAtomWin->GetCellValue(r,c);
      if(s!=_T(""))
      {
         double d;
         if(s.ToDouble(&d))
            mpMolecule->GetAtomList()[r]->SetY(d);
      }
   }
   if(c==4)
   {
      wxString s=mpAtomWin->GetCellValue(r,c);
      if(s!=_T(""))
      {
         double d;
         if(s.ToDouble(&d))
            mpMolecule->GetAtomList()[r]->SetZ(d);
      }
   }
   if(c==5)
   {
      wxString s=mpAtomWin->GetCellValue(r,c);
      if(s!=_T(""))
      {
         double d;
         if(s.ToDouble(&d))
         {
            if(d>1)d=1;
            else if(d<0)d=0;
            mpMolecule->GetAtomList()[r]->SetOccupancy(d);
         }
      }
   }
   mpMolecule->GetCrystal().UpdateDisplay();
   VFN_DEBUG_EXIT("WXMolecule::OnEditGridAtom():"<<e.GetRow()<<","<<e.GetCol(),10)
}

void WXMolecule::OnEditGridBondLength(wxGridEvent &e)
{
   if(mIsSelfUpdating) return;
   VFN_DEBUG_ENTRY("WXMolecule::OnEditGridBondLength():"<<e.GetRow()<<","<<e.GetCol(),10)
   const long r=e.GetRow();
   const long c=e.GetCol();
   if(c==0)
   {
      wxString s=mpBondWin->GetCellValue(r,c);
      vector<MolAtom*>::reverse_iterator at=mpMolecule->FindAtom(string(s.ToAscii()));
      if(at!=mpMolecule->GetAtomList().rend())
      {
         if(*at!=&(mpMolecule->GetBondList()[r]->GetAtom2()))
            mpMolecule->GetBondList()[r]->SetAtom1(**at);
         else
            mpBondWin->SetCellValue(r,c,wxString::FromAscii(mpMolecule->GetBondList()[r]->GetAtom1().GetName().c_str()));
      }
      else
         mpBondWin->SetCellValue(r,c,wxString::FromAscii(mpMolecule->GetBondList()[r]->GetAtom1().GetName().c_str()));
   }
   if(c==1)
   {
      wxString s=mpBondWin->GetCellValue(r,c);
      vector<MolAtom*>::reverse_iterator at=mpMolecule->FindAtom(string(s.ToAscii()));
      if(at!=mpMolecule->GetAtomList().rend())
      {
         if(*at!=&(mpMolecule->GetBondList()[r]->GetAtom1()))
            mpMolecule->GetBondList()[r]->SetAtom2(**at);
         else
            mpBondWin->SetCellValue(r,c,wxString::FromAscii(mpMolecule->GetBondList()[r]->GetAtom2().GetName().c_str()));
      }
      else
         mpBondWin->SetCellValue(r,c,wxString::FromAscii(mpMolecule->GetBondList()[r]->GetAtom2().GetName().c_str()));
   }
   if(c==3)
   {
      wxString s=mpBondWin->GetCellValue(r,c);
      if(s!=_T(""))
      {
         double d;
         if(s.ToDouble(&d))
            if(d>0) mpMolecule->GetBondList()[r]->SetLength0(d);
      }
   }
   if(c==4)
   {
      wxString s=mpBondWin->GetCellValue(r,c);
      if(s!=_T(""))
      {
         double d;
         if(s.ToDouble(&d))
            if(d>0) mpMolecule->GetBondList()[r]->SetLengthSigma(d);
      }
   }
   if(c==5)
   {
      wxString s=mpBondWin->GetCellValue(r,c);
      if(s!=_T(""))
      {
         double d;
         if(s.ToDouble(&d))
            if(d>0) mpMolecule->GetBondList()[r]->SetLengthDelta(d);
      }
   }
   this->CrystUpdate(true);
   VFN_DEBUG_EXIT("WXMolecule::OnEditGridBondLength():"<<e.GetRow()<<","<<e.GetCol(),10)
}

void WXMolecule::OnEditGridBondAngle(wxGridEvent &e)
{
   if(mIsSelfUpdating) return;
   VFN_DEBUG_ENTRY("WXMolecule::OnEditGridBondAngle():"<<e.GetRow()<<","<<e.GetCol(),10)
   const long r=e.GetRow();
   const long c=e.GetCol();
   if(c==0)
   {
      wxString s=mpAngleWin->GetCellValue(r,c);
      vector<MolAtom*>::reverse_iterator at=mpMolecule->FindAtom(string(s.ToAscii()));
      if(at!=mpMolecule->GetAtomList().rend())
      {
         if(  (*at!=&(mpMolecule->GetBondAngleList()[r]->GetAtom2()))
            &&(*at!=&(mpMolecule->GetBondAngleList()[r]->GetAtom3())))
            mpMolecule->GetBondAngleList()[r]->SetAtom1(**at);
         else
            mpAngleWin->SetCellValue(r,c,wxString::FromAscii(mpMolecule->GetBondAngleList()[r]->GetAtom1().GetName().c_str()));
      }
      else
         mpAngleWin->SetCellValue(r,c,wxString::FromAscii(mpMolecule->GetBondAngleList()[r]->GetAtom1().GetName().c_str()));
   }
   if(c==1)
   {
      wxString s=mpAngleWin->GetCellValue(r,c);
      vector<MolAtom*>::reverse_iterator at=mpMolecule->FindAtom(string(s.ToAscii()));
      if(at!=mpMolecule->GetAtomList().rend())
      {
         if(  (*at!=&(mpMolecule->GetBondAngleList()[r]->GetAtom1()))
            &&(*at!=&(mpMolecule->GetBondAngleList()[r]->GetAtom3())))
            mpMolecule->GetBondAngleList()[r]->SetAtom2(**at);
         else
            mpAngleWin->SetCellValue(r,c,wxString::FromAscii(mpMolecule->GetBondAngleList()[r]->GetAtom2().GetName().c_str()));
      }
      else
         mpAngleWin->SetCellValue(r,c,wxString::FromAscii(mpMolecule->GetBondAngleList()[r]->GetAtom2().GetName().c_str()));
   }
   if(c==2)
   {
      wxString s=mpAngleWin->GetCellValue(r,c);
      vector<MolAtom*>::reverse_iterator at=mpMolecule->FindAtom(string(s.ToAscii()));
      if(at!=mpMolecule->GetAtomList().rend())
      {
         if(  (*at!=&(mpMolecule->GetBondAngleList()[r]->GetAtom1()))
            &&(*at!=&(mpMolecule->GetBondAngleList()[r]->GetAtom2())))
            mpMolecule->GetBondAngleList()[r]->SetAtom3(**at);
         else
            mpAngleWin->SetCellValue(r,c,wxString::FromAscii(mpMolecule->GetBondAngleList()[r]->GetAtom3().GetName().c_str()));
      }
      else
         mpAngleWin->SetCellValue(r,c,wxString::FromAscii(mpMolecule->GetBondAngleList()[r]->GetAtom2().GetName().c_str()));
   }
   if(c==4)
   {
      wxString s=mpAngleWin->GetCellValue(r,c);
      if(s!=_T(""))
      {
         double d;
         if(s.ToDouble(&d))
            if(d>0) mpMolecule->GetBondAngleList()[r]->SetAngle0(d*DEG2RAD);
      }
   }
   if(c==5)
   {
      wxString s=mpAngleWin->GetCellValue(r,c);
      if(s!=_T(""))
      {
         double d;
         if(s.ToDouble(&d))
            if(d>0) mpMolecule->GetBondAngleList()[r]->SetAngleSigma(d*DEG2RAD);
      }
   }
   if(c==6)
   {
      wxString s=mpAngleWin->GetCellValue(r,c);
      if(s!=_T(""))
      {
         double d;
         if(s.ToDouble(&d))
            if(d>0) mpMolecule->GetBondAngleList()[r]->SetAngleDelta(d*DEG2RAD);
      }
   }
   this->CrystUpdate(true);
   VFN_DEBUG_EXIT("WXMolecule::OnEditGridBondAngle():"<<e.GetRow()<<","<<e.GetCol(),10)
}

void WXMolecule::OnEditGridDihedralAngle(wxGridEvent &e)
{
   if(mIsSelfUpdating) return;
   VFN_DEBUG_ENTRY("WXMolecule::OnEditGridDihedralAngle():"<<e.GetRow()<<","<<e.GetCol(),10)
   const long r=e.GetRow();
   const long c=e.GetCol();
   if(c==0)
   {
      wxString s=mpDihedralAngleWin->GetCellValue(r,c);
      vector<MolAtom*>::reverse_iterator at=mpMolecule->FindAtom(string(s.ToAscii()));
      if(at!=mpMolecule->GetAtomList().rend())
      {
         if(  (*at!=&(mpMolecule->GetDihedralAngleList()[r]->GetAtom2()))
            &&(*at!=&(mpMolecule->GetDihedralAngleList()[r]->GetAtom3())))
            mpMolecule->GetDihedralAngleList()[r]->SetAtom1(**at);
         else
            mpDihedralAngleWin->SetCellValue(r,c,wxString::FromAscii(mpMolecule->GetDihedralAngleList()[r]->GetAtom1().GetName().c_str()));
      }
      else
         mpDihedralAngleWin->SetCellValue(r,c,wxString::FromAscii(mpMolecule->GetDihedralAngleList()[r]->GetAtom1().GetName().c_str()));
   }
   if(c==1)
   {
      wxString s=mpDihedralAngleWin->GetCellValue(r,c);
      vector<MolAtom*>::reverse_iterator at=mpMolecule->FindAtom(string(s.ToAscii()));
      if(at!=mpMolecule->GetAtomList().rend())
      {
         if(  (*at!=&(mpMolecule->GetDihedralAngleList()[r]->GetAtom1()))
            &&(*at!=&(mpMolecule->GetDihedralAngleList()[r]->GetAtom3())))
            mpMolecule->GetDihedralAngleList()[r]->SetAtom2(**at);
         else
            mpDihedralAngleWin->SetCellValue(r,c,wxString::FromAscii(mpMolecule->GetDihedralAngleList()[r]->GetAtom2().GetName().c_str()));
      }
      else
         mpDihedralAngleWin->SetCellValue(r,c,wxString::FromAscii(mpMolecule->GetDihedralAngleList()[r]->GetAtom2().GetName().c_str()));
   }
   if(c==2)
   {
      wxString s=mpDihedralAngleWin->GetCellValue(r,c);
      vector<MolAtom*>::reverse_iterator at=mpMolecule->FindAtom(string(s.ToAscii()));
      if(at!=mpMolecule->GetAtomList().rend())
      {
         if(  (*at!=&(mpMolecule->GetDihedralAngleList()[r]->GetAtom1()))
            &&(*at!=&(mpMolecule->GetDihedralAngleList()[r]->GetAtom2())))
            mpMolecule->GetDihedralAngleList()[r]->SetAtom3(**at);
         else
            mpDihedralAngleWin->SetCellValue(r,c,wxString::FromAscii(mpMolecule->GetDihedralAngleList()[r]->GetAtom3().GetName().c_str()));
      }
      else
         mpDihedralAngleWin->SetCellValue(r,c,wxString::FromAscii(mpMolecule->GetDihedralAngleList()[r]->GetAtom3().GetName().c_str()));
   }
   if(c==3)
   {
      wxString s=mpDihedralAngleWin->GetCellValue(r,c);
      vector<MolAtom*>::reverse_iterator at=mpMolecule->FindAtom(string(s.ToAscii()));
      if(at!=mpMolecule->GetAtomList().rend())
      {
         if(  (*at!=&(mpMolecule->GetDihedralAngleList()[r]->GetAtom1()))
            &&(*at!=&(mpMolecule->GetDihedralAngleList()[r]->GetAtom2())))
            mpMolecule->GetDihedralAngleList()[r]->SetAtom4(**at);
         else
            mpDihedralAngleWin->SetCellValue(r,c,wxString::FromAscii(mpMolecule->GetDihedralAngleList()[r]->GetAtom4().GetName().c_str()));
      }
      else
         mpDihedralAngleWin->SetCellValue(r,c,wxString::FromAscii(mpMolecule->GetDihedralAngleList()[r]->GetAtom4().GetName().c_str()));
   }
   if(c==5)
   {
      wxString s=mpDihedralAngleWin->GetCellValue(r,c);
      if(s!=_T(""))
      {
         double d;
         if(s.ToDouble(&d))
            mpMolecule->GetDihedralAngleList()[r]->SetAngle0(d*DEG2RAD);
      }
   }
   if(c==6)
   {
      wxString s=mpDihedralAngleWin->GetCellValue(r,c);
      if(s!=_T(""))
      {
         double d;
         if(s.ToDouble(&d))
            if(d>0) mpMolecule->GetDihedralAngleList()[r]->SetAngleSigma(d*DEG2RAD);
      }
   }
   if(c==7)
   {
      wxString s=mpDihedralAngleWin->GetCellValue(r,c);
      if(s!=_T(""))
      {
         double d;
         if(s.ToDouble(&d))
            if(d>0) mpMolecule->GetDihedralAngleList()[r]->SetAngleDelta(d*DEG2RAD);
      }
   }
   this->CrystUpdate(true);
   VFN_DEBUG_EXIT("WXMolecule::OnEditGridDihedralAngle():"<<e.GetRow()<<","<<e.GetCol(),10)
}

void WXMolecule::OnEditGridRigidGroup(wxGridEvent &e)
{
   if(mIsSelfUpdating) return;
   VFN_DEBUG_ENTRY("WXMolecule::OnEditGridRigidGroup():"<<e.GetRow()<<","<<e.GetCol(),10)

   const long r=e.GetRow();
   const long c=e.GetCol();
   wxString s=mpRigidGroupWin->GetCellValue(r,c);
   list<string> l=SplitString(CompressString(string(s.ToAscii())," "),",");
   RigidGroup rg;
   for(list<string>::const_iterator pos=l.begin();pos!=l.end();++pos)
   {
      vector<MolAtom*>::reverse_iterator rpos=mpMolecule->FindAtom(*pos);
      if(rpos!=mpMolecule->GetAtomList().rend()) rg.insert(*rpos);
      else   cout<<*pos<<" : NOT FOUND"<<endl;;
   }
   set<MolAtom *> *pold=(set<MolAtom *>*)  mpMolecule->GetRigidGroupList()[r];
   set<MolAtom *> *pnew=(set<MolAtom *>*) &rg;

   if( *pold != *pnew)
   {
      *pold = *pnew;
      mpMolecule->GetRigidGroupClock().Click();
   }
   this->CrystUpdate(true);
   VFN_DEBUG_EXIT("WXMolecule::OnEditGridRigidGroup():"<<e.GetRow()<<","<<e.GetCol(),10)
}

void WXMolecule::OnMenuExport2ZMatrix(wxCommandEvent &event)
{
   VFN_DEBUG_MESSAGE("WXMolecule::OnMenuExport2ZMatrix()",6)
   const vector<MolZAtom> *pz=&(mpMolecule->AsZMatrix(true));

   if(event.GetId()==ID_MOLECULE_MENU_FILE_2ZMATRIX)
   {
      wxFileDialog open(this,_T("Choose a file to save the Z-matrix to"),_T(""),_T(""),_T("*.fhz"),
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
      if(open.ShowModal() != wxID_OK) return;
      ofstream fout (open.GetPath().ToAscii());
      fout.imbue(std::locale::classic());
      if(fout)
      {
         wxString tmp;

         fout<<mpMolecule->GetName()<<endl<<pz->size()<<endl;
         long i=0;
         for(vector<MolZAtom>::const_iterator pos=pz->begin();pos!=pz->end();++pos)
         {
            tmp.Printf(_T("%-2s %2lu"),pos->mpPow->GetSymbol().c_str(),pos->mBondAtom+1);
            fout<<tmp;
            if(i>0)
            {
               fout<<FormatFloat(pos->mBondLength,6,3);;
               if(i>1)
               {
                  tmp.Printf(_T(" %2lu"),pos->mBondAngleAtom+1);
                  fout<<tmp<<FormatFloat(pos->mBondAngle*RAD2DEG,8,3);;
                  if(i>2)
                  {
                     fout<<" "<<FormatInt(pos->mDihedralAtom+1,2)<<FormatFloat(pos->mDihedralAngle*RAD2DEG,8,3);
                  }
               }
            }
            fout<<endl;
            i++;
         }
      }
      fout.close();
   }
   else
   {
      wxFileDialog open(this,_T("Choose a file to save the (named) Z-matrix to"),_T(""),_T(""),_T("*.zmat"),
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

      if(open.ShowModal() != wxID_OK) return;

      unsigned long nbchar=0;
      for(vector<MolAtom*>::const_iterator pos=mpMolecule->GetAtomList().begin();
          pos!=mpMolecule->GetAtomList().end();++pos)
            if(nbchar<(*pos)->GetName().size()) nbchar=(*pos)->GetName().size();

      ofstream fout (open.GetPath().ToAscii());
      fout.imbue(std::locale::classic());
      if(fout)
      {
         wxString tmp;

         fout<<mpMolecule->GetName()<<endl<<pz->size()<<endl;
         long i=0;
         for(vector<MolZAtom>::const_iterator pos=pz->begin();pos!=pz->end();++pos)
         {
            fout.width(nbchar);
            fout<<mpMolecule->GetAtomList()[i]->GetName();
            tmp.Printf(_T(" %2s "),pos->mpPow->GetSymbol().c_str());
            fout<<tmp;
            fout.width(nbchar);
            fout<<mpMolecule->GetAtomList()[pos->mBondAtom]->GetName()<<" ";
            if(i>0)
            {
               fout<<FormatFloat(pos->mBondLength,6,3)<<" ";
               if(i>1)
               {
                  fout.width(nbchar);
                  fout<<mpMolecule->GetAtomList()[pos->mBondAngleAtom]->GetName();
                  fout<<" "<<FormatFloat(pos->mBondAngle*RAD2DEG,8,3)<<" ";
                  if(i>2)
                  {
                     fout.width(nbchar);
                     fout<<mpMolecule->GetAtomList()[pos->mDihedralAtom]->GetName();
                     fout<<" "<<FormatFloat(pos->mDihedralAngle*RAD2DEG,8,3)<<" ";
                  }
               }
            }
            fout<<endl;
            i++;
         }
      }
      fout.close();
   }
}

void WXMolecule::OnMenuTest(wxCommandEvent & WXUNUSED(event))
{
   VFN_DEBUG_ENTRY("WXMolecule::OnMenuTest()",6)
   #if 1
   mpMolecule->BuildRingList();
   mpMolecule->BuildStretchModeBondLength();
   mpMolecule->BuildStretchModeBondAngle();
   mpMolecule->BuildStretchModeTorsion();
   mpMolecule->BuildStretchModeTwist();

   //mpMolecule->BuildRotorGroup();
   mpMolecule->BuildFlipGroup();
   mpMolecule->TuneGlobalOptimRotationAmplitude();
   //mpMolecule->FlipAtomGroup(*(mpMolecule->mvFlipGroup.begin()));
   //mpMolecule->GlobalOptRandomMove(0.1,gpRefParTypeObjCryst);
   mpMolecule->BuildStretchModeGroups();
   #else
   for(list<StretchModeTorsion>::iterator pos=mpMolecule->GetStretchModeTorsionList().begin();
       pos!=mpMolecule->GetStretchModeTorsionList().end();++pos)
   {
      if((pos->mpAtom1->GetName()=="C12")&&(pos->mpAtom2->GetName()=="N13"))
      {
         //mpMolecule->DihedralAngleRandomChange(*pos,0.01,true);
         cout<<pos->mpDihedralAngle->GetAngle()*RAD2DEG;
         mpMolecule->RotateAtomGroup(*(pos->mpAtom1),*(pos->mpAtom2),pos->mvRotatedAtomList,2*M_PI/10,true);
         cout<<" -> "<<pos->mpDihedralAngle->GetAngle()*RAD2DEG
             <<", llk="<<pos->mpDihedralAngle->GetLogLikelihood()<<endl;
      }
   }
   mpMolecule->GetCrystal().UpdateDisplay();
   #endif
   #if 0
   mpMolecule->BeginOptimization(true);
   for(REAL amplitude=0.1;amplitude<10;amplitude*=1.5)
   {
      REAL maxLLK=0,llk,ave=0.0;
      for(unsigned long i=0;i<1000;i++)
      {
         mpMolecule->BeginGlobalOptRandomMove();
         mpMolecule->GlobalOptRandomMove(amplitude,gpRefParTypeObjCryst);
         llk=mpMolecule->GetLogLikelihood();
         //cout<<"           "<<llk<<endl;
         if(llk>maxLLK) maxLLK=llk;
         ave+=llk;
         mpMolecule->GetCrystal().UpdateDisplay();
      }
      cout<<"Amplitude="<<amplitude<<", <LLK>= "<<ave/1000<<", Max LLK= "<<maxLLK<<endl;
   }
   mpMolecule->EndOptimization();
   #endif
   VFN_DEBUG_EXIT("WXMolecule::OnMenuTest()",6)
}

class MoleculeMDTestThread: public wxThread
{
public:
   MoleculeMDTestThread(Molecule  &mol,float seconds):
   wxThread(wxTHREAD_DETACHED),mpMolecule(&mol),mSeconds(seconds){};
   virtual void *Entry()
   {
      cout<<endl<<"Entering refinement thread "<<endl<<endl;
      const map<MolAtom *,set<MolAtom *> > *pConnect=&(mpMolecule-> GetConnectivityTable());
      Chronometer chrono;
      float dt0=chrono.seconds();
      unsigned long ct=0;
      while(chrono.seconds()<30)
      {
         cout<<"MD Test, cycle #"<<ct++<<endl;
         // Use Same procedure as in Molecule::GLobalOptRandomMove
         #if 0
         // Use one center for the position of an impulsion, applied to all atoms with an exponential decrease
         REAL xmin=(*mpMolecule->mvMDFullAtomGroup.begin())->GetX();
         REAL xmax=(*mpMolecule->mvMDFullAtomGroup.begin())->GetX();
         REAL ymin=(*mpMolecule->mvMDFullAtomGroup.begin())->GetY();
         REAL ymax=(*mpMolecule->mvMDFullAtomGroup.begin())->GetY();
         REAL zmin=(*mpMolecule->mvMDFullAtomGroup.begin())->GetZ();
         REAL zmax=(*mpMolecule->mvMDFullAtomGroup.begin())->GetZ();
         for(set<MolAtom*>::iterator at=mpMolecule->mvMDFullAtomGroup.begin();at!=mpMolecule->mvMDFullAtomGroup.end();++at)
         {
            if((*at)->GetX()<xmin) xmin=(*at)->GetX();
            if((*at)->GetX()>xmax) xmax=(*at)->GetX();
            if((*at)->GetY()<ymin) ymin=(*at)->GetY();
            if((*at)->GetY()>ymax) ymax=(*at)->GetY();
            if((*at)->GetZ()<zmin) zmin=(*at)->GetZ();
            if((*at)->GetZ()>zmax) zmax=(*at)->GetZ();
         }
         // Apply a gaussian impulsion to part of the atom group (FWHM=1/5 of group size)
         REAL dx=(xmax-xmin)/5.,dy=(ymax-ymin)/5.,dz=(zmax-zmin)/5.;
         if(dx<2) dx=2;
         if(dy<2) dy=2;
         if(dz<2) dz=2;
         const REAL xc=xmin+rand()/(REAL)RAND_MAX*(xmax-xmin);
         const REAL yc=ymin+rand()/(REAL)RAND_MAX*(ymax-ymin);
         const REAL zc=zmin+rand()/(REAL)RAND_MAX*(zmax-zmin);
         map<MolAtom*,XYZ> v0;
         const REAL ax=-4.*log(2.)/(dx*dx);
         const REAL ay=-4.*log(2.)/(dy*dy);
         const REAL az=-4.*log(2.)/(dz*dz);
         REAL ux,uy,uz,n=0;
         while(n<1)
         {
            ux=REAL(rand()-RAND_MAX/2);
            uy=REAL(rand()-RAND_MAX/2);
            uz=REAL(rand()-RAND_MAX/2);
            n=sqrt(ux*ux+uy*uy+uz*uz);
         }
         ux=ux/n;uy=uy/n;uz=uz/n;
         if(rand()%2==0)
            for(set<MolAtom*>::iterator at=mpMolecule->mvMDFullAtomGroup.begin();at!=mpMolecule->mvMDFullAtomGroup.end();++at)
               v0[*at]=XYZ(ux*exp(ax*((*at)->GetX()-xc)*((*at)->GetX()-xc)),
                           uy*exp(ay*((*at)->GetY()-yc)*((*at)->GetY()-yc)),
                           uz*exp(az*((*at)->GetZ()-zc)*((*at)->GetZ()-zc)));
         else
            for(set<MolAtom*>::iterator at=mpMolecule->mvMDFullAtomGroup.begin();at!=mpMolecule->mvMDFullAtomGroup.end();++at)
               v0[*at]=XYZ(((*at)->GetX()-xc)*ux*exp(ax*((*at)->GetX()-xc)*((*at)->GetX()-xc)),
                           ((*at)->GetY()-yc)*uy*exp(ay*((*at)->GetY()-yc)*((*at)->GetY()-yc)),
                           ((*at)->GetZ()-zc)*uz*exp(az*((*at)->GetZ()-zc)*((*at)->GetZ()-zc)));
         #else
         map<MolAtom*,XYZ> v0;
         // Use one atom for the center of the impulsion, 'push' atoms depending on distance & connectivity table
         for(set<MolAtom*>::iterator at=mpMolecule->mvMDFullAtomGroup.begin();at!=mpMolecule->mvMDFullAtomGroup.end();++at)
            v0[*at]=XYZ(0,0,0);
         map<MolAtom*,unsigned long> pushedAtoms;
         unsigned long idx=rand()%v0.size();
         set<MolAtom*>::iterator at0=mpMolecule->mvMDFullAtomGroup.begin();
         for(unsigned int i=0;i<idx;i++) at0++;
         const REAL xc=(*at0)->GetX();
         const REAL yc=(*at0)->GetY();
         const REAL zc=(*at0)->GetZ();
         ExpandAtomGroupRecursive(*at0,*pConnect,pushedAtoms,3);
         REAL ux,uy,uz,n=0;
         while(n<1)
         {
            ux=REAL(rand()-RAND_MAX/2);
            uy=REAL(rand()-RAND_MAX/2);
            uz=REAL(rand()-RAND_MAX/2);
            n=sqrt(ux*ux+uy*uy+uz*uz);
         }
         ux=ux/n;uy=uy/n;uz=uz/n;
         const REAL a=-4.*log(2.)/(2*2);//FWHM=2 Angstroems
         if(rand()%2==0)
            for(map<MolAtom*,unsigned long>::iterator at=pushedAtoms.begin() ;at!=pushedAtoms.end();++at)
               v0[at->first]=XYZ(ux*exp(a*(at->first->GetX()-xc)*(at->first->GetX()-xc)),
                           uy*exp(a*(at->first->GetY()-yc)*(at->first->GetY()-yc)),
                           uz*exp(a*(at->first->GetZ()-zc)*(at->first->GetZ()-zc)));
         else
            for(map<MolAtom*,unsigned long>::iterator at=pushedAtoms.begin() ;at!=pushedAtoms.end();++at)
               v0[at->first]=XYZ((at->first->GetX()-xc)*ux*exp(a*(at->first->GetX()-xc)*(at->first->GetX()-xc)),
                                 (at->first->GetY()-yc)*uy*exp(a*(at->first->GetY()-yc)*(at->first->GetY()-yc)),
                                 (at->first->GetZ()-zc)*uz*exp(a*(at->first->GetZ()-zc)*(at->first->GetZ()-zc)));

         #endif
         const REAL nrj0=40*( mpMolecule->GetBondList().size()
                             +mpMolecule->GetBondAngleList().size()
                             +mpMolecule->GetDihedralAngleList().size());
         map<RigidGroup*,std::pair<XYZ,XYZ> > vr;
         mpMolecule->MolecularDynamicsEvolve(v0, 200,0.004,
                                             mpMolecule->GetBondList(),
                                             mpMolecule->GetBondAngleList(),
                                             mpMolecule->GetDihedralAngleList(),
                                             vr,nrj0);

         // Display
         wxMilliSleep(1);// Slow down display for simple structures
         if((chrono.seconds()-dt0)>0.05) {mpMolecule->GetCrystal().UpdateDisplay();dt0=chrono.seconds();}
      }
      return NULL;
   };
   virtual void OnExit()
   {
      cout <<endl<<"Exiting refinement thread "<<endl<<endl;
   };
private:
   /// The molecule to randomly
   Molecule *mpMolecule;
   /// Test duration
   float mSeconds;
};

void WXMolecule::OnMenuMDTest(wxCommandEvent & WXUNUSED(event))
{
   VFN_DEBUG_ENTRY("WXMolecule::OnMenuMDTest()",6)
   {
      wxProgressDialog dlgProgress(_T("Beginning MD Test..."),_T("Building Flip Groups"),
                                   7,this,wxPD_AUTO_HIDE|wxPD_ELAPSED_TIME);//|wxPD_CAN_ABORT
      mpMolecule->BuildFlipGroup();
      dlgProgress.Update(0,_T("Build Ring List")) ;
      mpMolecule->BuildRingList();
      dlgProgress.Update(1,_T("Build Stretch Modes: Bond Length")) ;
      mpMolecule->BuildStretchModeBondLength();
      dlgProgress.Update(2,_T("Build Stretch Modes: Bond Angle")) ;
      mpMolecule->BuildStretchModeBondAngle();
      dlgProgress.Update(3,_T("Build Stretch Modes: Torsion")) ;
      mpMolecule->BuildStretchModeTorsion();
      //mpMolecule->BuildStretchModeTwist();
      dlgProgress.Update(4,_T("Build Stretch Modes: Optimize Rotation Amplitudes")) ;
      mpMolecule->TuneGlobalOptimRotationAmplitude();
      dlgProgress.Update(5,_T("Build Stretch Modes Groups")) ;
      mpMolecule->BuildStretchModeGroups();
      dlgProgress.Update(6,_T("Build MD Atom Groups")) ;
      mpMolecule->BuildMDAtomGroups();
   }

   MoleculeMDTestThread *pTest = new MoleculeMDTestThread(*mpMolecule,30);
   if(pTest->Create() != wxTHREAD_NO_ERROR)
      wxLogError(_T("Can't create test optimization thread"));
   else pTest->Run();
   VFN_DEBUG_EXIT("WXMolecule::OnMenuMDTest()",6)
}

static const long ID_MOLECULE_ROTATE_BOND_GO   =WXCRYST_ID();
static const long ID_MOLECULE_ROTATE_BOND_ATOMS=WXCRYST_ID();

class WXMoleculeRotation:public wxWindow
{
   public:
      WXMoleculeRotation(wxWindow *parent, Molecule &mol):
      wxWindow(parent,-1),mBondListClock(mol.GetBondListClock()),mpMol(&mol)
      {
         VFN_DEBUG_ENTRY("WXMoleculeRotation::WXMoleculeRotation()",10)
         this->SetFont(wxFont(8,wxTELETYPE,wxFONTSTYLE_NORMAL,wxFONTWEIGHT_NORMAL));

         wxArrayString choices;
         for(vector<MolBond*>::const_iterator pos=mol.GetBondList().begin();pos!=mol.GetBondList().end();++pos)
            choices.Add(wxString::FromAscii((*pos)->GetName().c_str()));

         wxBoxSizer* pSizer=new wxBoxSizer(wxHORIZONTAL);

         wxBoxSizer* pSizer1=new wxBoxSizer(wxVERTICAL);
         pSizer1->Add(new wxStaticText(this,-1,_T("Select bond:")),0,wxALIGN_CENTER);
         mpBond= new wxChoice(this,ID_MOLECULE_ROTATE_BOND_ATOMS,wxDefaultPosition,wxDefaultSize,choices);
         mpBond->SetSelection(0);
         pSizer1->Add(mpBond,0,wxALIGN_CENTER);
         pSizer->Add(pSizer1,0,wxALIGN_CENTER);

         wxBoxSizer* pSizer2=new wxBoxSizer(wxVERTICAL);
         pSizer2->Add(new wxStaticText(this,-1,_T("Atoms to rotate:")),0,wxALIGN_CENTER);
         mpRotatedAtoms= new wxListBox(this,-1,wxDefaultPosition,wxSize(150,60));
         pSizer2->Add(mpRotatedAtoms,0,wxALIGN_CENTER);
         pSizer->Add(pSizer2,0,wxALIGN_CENTER);

         wxBoxSizer* pSizer3=new wxBoxSizer(wxVERTICAL);
         pSizer3->Add(new wxStaticText(this,-1,_T("Amplitude:")),0,wxALIGN_CENTER);
         mpAngle=new wxTextCtrl(this,-1,_T("10"));
         pSizer3->Add(mpAngle,0,wxALIGN_CENTER);
         pSizer->Add(pSizer3,0,wxALIGN_CENTER);

         wxButton *pButtonRotate=new wxButton(this,ID_MOLECULE_ROTATE_BOND_GO,_T("Rotate !"));
         pSizer->Add(pButtonRotate,0,wxALIGN_CENTER);

         this->SetSizer(pSizer);
         this->SetAutoLayout(true);
         pSizer->SetSizeHints(this);
         pSizer->SetSizeHints(parent);
         this->Layout();
         wxCommandEvent ev(wxEVT_COMMAND_CHOICE_SELECTED,ID_MOLECULE_ROTATE_BOND_ATOMS);
         wxPostEvent(this,ev);
         VFN_DEBUG_EXIT("WXMoleculeRotation::WXMoleculeRotation()",10)
      }

      void OnRotate(wxCommandEvent &event)
      {
         if(mBondListClock<mpMol->GetBondListClock())
         {
            cout<<" The bond list has changed !"<<endl;
            this->GetParent()->Destroy();
            return;
         }
         VFN_DEBUG_MESSAGE("WXMoleculeRotation::OnRotate()",10)
         unsigned int bond=mpBond->GetSelection();
         unsigned int choice=mpRotatedAtoms->GetSelection();
         double angle;
         mpAngle->GetValue().ToDouble(&angle);
         mpMol->RotateAtomGroup(mpMol->GetBondList()[bond]->GetAtom1(),
                                mpMol->GetBondList()[bond]->GetAtom2(),
                                mvpRotatedAtoms[choice], angle*DEG2RAD, true);
         mpMol->GetCrystal().UpdateDisplay();
      }
      void OnSelectBond(wxCommandEvent &event)
      {
         if(mBondListClock<mpMol->GetBondListClock())
         {
            cout<<" The bond list has changed !"<<endl;
            this->GetParent()->Destroy();
            return;
         }
         VFN_DEBUG_MESSAGE("WXMoleculeRotation::OnSelectBond()",10)
         const unsigned int choice=mpBond->GetSelection();
         MolAtom *pAt1=&(mpMol->GetBondList()[choice]->GetAtom1());
         MolAtom *pAt2=&(mpMol->GetBondList()[choice]->GetAtom2());
         mpMol->BuildConnectivityTable();

         mvpRotatedAtoms.clear();
         mvpRotatedAtoms.resize(2);
         mvpRotatedAtoms[0].insert(pAt1);
         ExpandAtomGroupRecursive(pAt2,mpMol->GetConnectivityTable(),mvpRotatedAtoms[0]);

         mvpRotatedAtoms[1].insert(pAt2);
         ExpandAtomGroupRecursive(pAt1,mpMol->GetConnectivityTable(),mvpRotatedAtoms[1]);

         wxArrayString choices;

         set<MolAtom *>::const_iterator pos=mvpRotatedAtoms[0].begin();
         wxString choice1(wxString::FromAscii((*pos++)->GetName().c_str()));
         for(;pos!=mvpRotatedAtoms[0].end();++pos)
            choice1 +=_T("-")+wxString::FromAscii((*pos)->GetName().c_str());
         choices.Add(choice1);

         pos=mvpRotatedAtoms[1].begin();
         wxString choice2(wxString::FromAscii((*pos++)->GetName().c_str()));
         for(;pos!=mvpRotatedAtoms[1].end();++pos)
            choice2 +=_T("-")+wxString::FromAscii((*pos)->GetName().c_str());
         choices.Add(choice2);

         mpRotatedAtoms->Set(choices);
         mpRotatedAtoms->SetSelection(0);
      }
   private:
      /// Record the last time the bond list was changed
      const RefinableObjClock& mBondListClock;
      Molecule *mpMol;
      wxChoice *mpBond;
      wxListBox *mpRotatedAtoms;
      vector<set<MolAtom *> > mvpRotatedAtoms;
      wxTextCtrl *mpAngle;
      DECLARE_EVENT_TABLE()
};
BEGIN_EVENT_TABLE(WXMoleculeRotation,wxWindow)
   EVT_BUTTON(ID_MOLECULE_ROTATE_BOND_GO, WXMoleculeRotation::OnRotate)
   EVT_CHOICE(ID_MOLECULE_ROTATE_BOND_ATOMS, WXMoleculeRotation::OnSelectBond)
END_EVENT_TABLE()

static const long ID_MOLECULE_ROTATE_DIHED_GO   =WXCRYST_ID();
static const long ID_MOLECULE_ROTATE_DIHED_ATOMS=WXCRYST_ID();

class WXMoleculeRotationDihed:public wxWindow
{
   public:
      WXMoleculeRotationDihed(wxWindow *parent, Molecule &mol):
      wxWindow(parent,-1),mBondListClock(mol.GetBondListClock()),mpMol(&mol)
      {
         VFN_DEBUG_ENTRY("WXMoleculeRotationDihed::WXMoleculeRotationDihed()",10)
         this->SetFont(wxFont(8,wxTELETYPE,wxFONTSTYLE_NORMAL,wxFONTWEIGHT_NORMAL));

         wxArrayString choices;
         // Use existing angle restraints to generate dihedral angles
         for(vector<MolBondAngle*>::const_iterator pos=mol.GetBondAngleList().begin();pos!=mol.GetBondAngleList().end();++pos)
         {
            MolAtom *pAt1=&((*pos)->GetAtom1());
            MolAtom *pAt2=&((*pos)->GetAtom2());
            MolAtom *pAt3=&((*pos)->GetAtom3());
            const set<MolAtom * > *pConn=&(mpMol->GetConnectivityTable().find(pAt3)->second);
            for(set<MolAtom * >::const_iterator neigh=pConn->begin();neigh!=pConn->end();++neigh)
            {
               if( (*neigh==pAt1) || (*neigh==pAt2) ) continue;
               mvDihed.push_back(MolDihedralAngle(*pAt1,*pAt2,*pAt3,**neigh,0,.001,.001,*mpMol));
               choices.Add(wxString::FromAscii(mvDihed.back().GetName().c_str()));
            }
         }

         wxBoxSizer* pSizer=new wxBoxSizer(wxHORIZONTAL);

         wxBoxSizer* pSizer1=new wxBoxSizer(wxVERTICAL);
         pSizer1->Add(new wxStaticText(this,-1,_T("Dihedral Angle:")),0,wxALIGN_CENTER);
         mpDihed= new wxChoice(this,ID_MOLECULE_ROTATE_DIHED_ATOMS,wxDefaultPosition,wxDefaultSize,choices);
         mpDihed->SetSelection(0);
         pSizer1->Add(mpDihed,0,wxALIGN_CENTER);
         pSizer->Add(pSizer1,0,wxALIGN_CENTER);

         wxBoxSizer* pSizer2=new wxBoxSizer(wxVERTICAL);
         pSizer2->Add(new wxStaticText(this,-1,_T("Atoms to rotate:")),0,wxALIGN_CENTER);
         mpRotatedAtoms= new wxListBox(this,-1,wxDefaultPosition,wxSize(150,60));
         pSizer2->Add(mpRotatedAtoms,0,wxALIGN_CENTER);
         pSizer->Add(pSizer2,0,wxALIGN_CENTER);

         wxBoxSizer* pSizer3=new wxBoxSizer(wxVERTICAL);
         pSizer3->Add(new wxStaticText(this,-1,_T("Angle:")),0,wxALIGN_CENTER);
         mpAngle=new wxTextCtrl(this,-1,_T("10"));
         pSizer3->Add(mpAngle,0,wxALIGN_CENTER);
         pSizer->Add(pSizer3,0,wxALIGN_CENTER);

         wxButton *pButtonRotate=new wxButton(this,ID_MOLECULE_ROTATE_DIHED_GO,_T("Set angle !"));
         pSizer->Add(pButtonRotate,0,wxALIGN_CENTER);

         this->SetSizer(pSizer);
         this->SetAutoLayout(true);
         pSizer->SetSizeHints(this);
         pSizer->SetSizeHints(parent);
         this->Layout();
         wxCommandEvent ev(wxEVT_COMMAND_CHOICE_SELECTED,ID_MOLECULE_ROTATE_DIHED_ATOMS);
         wxPostEvent(this,ev);
         VFN_DEBUG_EXIT("WXMoleculeRotationDihed::WXMoleculeRotationDihed()",10)
      }

      void OnRotate(wxCommandEvent &event)
      {
         if(mBondListClock<mpMol->GetBondListClock())
         {
            cout<<" The bond list has changed !"<<endl;
            this->GetParent()->Destroy();
            return;
         }
         VFN_DEBUG_MESSAGE("WXMoleculeRotationDihed::OnRotate()",10)
         MolDihedralAngle *pDihed=&mvDihed[mpDihed->GetSelection()];
         unsigned int choice=mpRotatedAtoms->GetSelection();
         double angle;
         mpAngle->GetValue().ToDouble(&angle);
         angle*=DEG2RAD;
         angle-=pDihed->GetAngle();

         if(mvpRotatedAtoms[choice].find(&(pDihed->GetAtom1()))!=mvpRotatedAtoms[choice].end()) angle*=-1;

         mpMol->RotateAtomGroup(pDihed->GetAtom2(),
                                pDihed->GetAtom3(),
                                mvpRotatedAtoms[choice], angle, true);
         mpMol->GetCrystal().UpdateDisplay();
      }
      void OnSelectDihed(wxCommandEvent &event)
      {
         if(mBondListClock<mpMol->GetBondListClock())
         {
            cout<<" The bond list has changed !"<<endl;
            this->GetParent()->Destroy();
            return;
         }
         VFN_DEBUG_MESSAGE("WXMoleculeRotationDihed::OnSelectBond()",10)
         MolDihedralAngle *pDihed=&mvDihed[mpDihed->GetSelection()];
         MolAtom *pAt1=&(pDihed->GetAtom1());
         MolAtom *pAt2=&(pDihed->GetAtom2());
         MolAtom *pAt3=&(pDihed->GetAtom3());
         MolAtom *pAt4=&(pDihed->GetAtom4());
         mpMol->BuildConnectivityTable();

         mvpRotatedAtoms.clear();
         mvpRotatedAtoms.resize(2);

         mvpRotatedAtoms[0].insert(pAt1);
         mvpRotatedAtoms[0].insert(pAt2);
         ExpandAtomGroupRecursive(pAt3,mpMol->GetConnectivityTable(),mvpRotatedAtoms[0]);
         mvpRotatedAtoms[0].erase(pAt1);
         mvpRotatedAtoms[0].erase(pAt2);

         mvpRotatedAtoms[1].insert(pAt4);
         mvpRotatedAtoms[1].insert(pAt3);
         ExpandAtomGroupRecursive(pAt2,mpMol->GetConnectivityTable(),mvpRotatedAtoms[1]);
         mvpRotatedAtoms[1].erase(pAt4);
         mvpRotatedAtoms[1].erase(pAt3);

         wxArrayString choices;

         set<MolAtom *>::const_iterator pos=mvpRotatedAtoms[0].begin();
         wxString choice1(wxString::FromAscii((*pos++)->GetName().c_str()));
         for(;pos!=mvpRotatedAtoms[0].end();++pos)
            choice1 +=_T("-")+wxString::FromAscii((*pos)->GetName().c_str());
         choices.Add(choice1);

         pos=mvpRotatedAtoms[1].begin();
         wxString choice2(wxString::FromAscii((*pos++)->GetName().c_str()));
         for(;pos!=mvpRotatedAtoms[1].end();++pos)
            choice2 +=_T("-")+wxString::FromAscii((*pos)->GetName().c_str());
         choices.Add(choice2);

         mpRotatedAtoms->Set(choices);
         mpRotatedAtoms->SetSelection(0);
         mpAngle->SetValue(wxString::Format(_T("%6.2f"),pDihed->GetAngle()*RAD2DEG));
      }
   private:
      /// Record the last time the bond list was changed
      const RefinableObjClock& mBondListClock;
      Molecule *mpMol;
      vector<MolDihedralAngle > mvDihed;
      wxChoice *mpDihed;
      wxListBox *mpRotatedAtoms;
      vector<set<MolAtom *> > mvpRotatedAtoms;
      wxTextCtrl *mpAngle;
      DECLARE_EVENT_TABLE()
};
BEGIN_EVENT_TABLE(WXMoleculeRotationDihed,wxWindow)
   EVT_BUTTON(ID_MOLECULE_ROTATE_DIHED_GO, WXMoleculeRotationDihed::OnRotate)
   EVT_CHOICE(ID_MOLECULE_ROTATE_DIHED_ATOMS, WXMoleculeRotationDihed::OnSelectDihed)
END_EVENT_TABLE()

void WXMolecule::OnMenuRotate(wxCommandEvent &event)
{
   VFN_DEBUG_ENTRY("WXMolecule::OnMenuRotate()",10)
   if(event.GetId()==ID_MOLECULE_MENU_GEOMETRY_ROTATE_BOND)
   {
      #ifdef __WXGTK__
      wxFrame *frame= new wxMiniFrame(this,-1,_T("Rotate around bond"),wxDefaultPosition,
                                          wxDefaultSize,wxCLOSE_BOX|wxSTAY_ON_TOP|wxCAPTION);
      #else
      wxFrame *frame= new wxFrame(this,-1,_T("Rotate around bond"),wxDefaultPosition,
                                          wxDefaultSize);
      #endif
      WXMoleculeRotation * wxMolRot;
      wxMolRot=new WXMoleculeRotation(frame,*mpMolecule);
      frame->Show(true);
   }
   if(event.GetId()==ID_MOLECULE_MENU_GEOMETRY_ROTATE_DIHED)
   {
      #ifdef __WXGTK__
      wxFrame *frame= new wxMiniFrame(this,-1,_T("Change dihedral angle"),wxDefaultPosition,
                                          wxDefaultSize,wxCLOSE_BOX|wxSTAY_ON_TOP|wxCAPTION);
      #else
      wxFrame *frame= new wxFrame(this,-1,_T("Change dihedral angle"),wxDefaultPosition,
                                          wxDefaultSize);
      #endif
      WXMoleculeRotationDihed * wxMolRot;
      wxMolRot=new WXMoleculeRotationDihed(frame,*mpMolecule);
      frame->Show(true);
   }
   VFN_DEBUG_EXIT("WXMolecule::OnMenuRotate()",10)
}


void WXMolecule::OnMenuSetLimits(wxCommandEvent &event)
{
}

void WXMolecule::CrystUpdate(const bool uui,const bool lock)
{
   VFN_DEBUG_ENTRY("WXMolecule::CrystUpdate()",6)
   if(lock) mMutex.Lock();
   if(false==mpMolecule->IsBeingRefined())
   {
      //Remove any atom, bond, bond angle or dihedral angle that could have been removed
      {
         unsigned long i=0;
         for(list<CellAtom>::iterator pos=mvpAtom.begin();pos!=mvpAtom.end();)
         {
            if(i>=mpMolecule->GetAtomList().size())
            {
               pos=mvpAtom.erase(pos);
               mpAtomWin->DeleteRows(i);
            }
            else
            {
               if(pos->mpAtom!=mpMolecule->GetAtomList()[i])
               {
                  pos=mvpAtom.erase(pos);
                  mpAtomWin->DeleteRows(i);
               }
               else
               {
                  ++pos;
                  ++i;
               }
            }
         }
      }
      if(0!=mpBondWin)
      {
         unsigned long i=0;
         for(list<CellBond>::iterator pos=mvpBond.begin();pos!=mvpBond.end();)
         {
            if(i>=mpMolecule->GetBondList().size())
            {
               pos=mvpBond.erase(pos);
               mpBondWin->DeleteRows(i);
            }
            else
            {
               if(pos->mpBond!=mpMolecule->GetBondList()[i])
               {
                  pos=mvpBond.erase(pos);
                  mpBondWin->DeleteRows(i);
               }
               else
               {
                  ++pos;
                  ++i;
               }
            }
         }
      }
      if(0!=mpAngleWin)
      {
         unsigned long i=0;
         for(list<CellBondAngle>::iterator pos=mvpBondAngle.begin();pos!=mvpBondAngle.end();)
         {
            if(i>=mpMolecule->GetBondAngleList().size())
            {
               pos=mvpBondAngle.erase(pos);
               mpAngleWin->DeleteRows(i);
            }
            else
            {
               if(pos->mpBondAngle!=mpMolecule->GetBondAngleList()[i])
               {
                  pos=mvpBondAngle.erase(pos);
                  mpAngleWin->DeleteRows(i);
               }
               else
               {
                  ++pos;
                  ++i;
               }
            }
         }
      }
      if(0!=mpDihedralAngleWin)
      {
         unsigned long i=0;
         for(list<CellDihedralAngle>::iterator pos=mvpDihedralAngle.begin();pos!=mvpDihedralAngle.end();)
         {
            if(i>=mpMolecule->GetDihedralAngleList().size())
            {
               pos=mvpDihedralAngle.erase(pos);
               mpDihedralAngleWin->DeleteRows(i);
            }
            else
            {
               if(pos->mpDihedralAngle!=mpMolecule->GetDihedralAngleList()[i])
               {
                  pos=mvpDihedralAngle.erase(pos);
                  mpDihedralAngleWin->DeleteRows(i);
               }
               else
               {
                  ++pos;
                  ++i;
               }
            }
         }
      }
      if(0!=mpRigidGroupWin)
      {
         unsigned long i=0;
         for(list<CellRigidGroup>::iterator pos=mvpRigidGroup.begin();pos!=mvpRigidGroup.end();)
         {
            if(i>=mpMolecule->GetRigidGroupList().size())
            {
               pos=mvpRigidGroup.erase(pos);
               mpRigidGroupWin->DeleteRows(i);
            }
            else
            {
               if(pos->mpGroup!=mpMolecule->GetRigidGroupList()[i])
               {
                  pos=mvpRigidGroup.erase(pos);
                  mpRigidGroupWin->DeleteRows(i);
               }
               else
               {
                  ++pos;
                  ++i;
               }
            }
         }
      }
      if(0!=mpNonFlipAtomWin)
      {
         if(mpNonFlipAtomWin->GetNumberRows()>0)
            mpNonFlipAtomWin->DeleteRows(0, mpNonFlipAtomWin->GetRows(), true);
      }
      //Add any atom, bond, bond angle or dihedral angle that could have been added
      {
         bool needLayout=false;
         for(unsigned long i=mvpAtom.size();i<mpMolecule->GetAtomList().size();++i)
         {
            VFN_DEBUG_MESSAGE("WXMolecule::CrystUpdate():Atom not found",5)
            mpAtomWin->AppendRows();
            mvpAtom.push_back(CellAtom());
            mvpAtom.back().mpAtom=mpMolecule->GetAtomList()[i];
            needLayout=true;
         }
         if(needLayout)
         {
            //mpAtomWin->Layout();
            //mpAtomWin->SetScrollRate(20,20);
            mpAtomWin->FitInside();
            //mpAtomWin->EnableScrolling(true,true);
         }
      }
      if(0!=mpBondWin)
      {
         for(unsigned long i=mvpBond.size();i<mpMolecule->GetBondList().size();++i)
         {
            VFN_DEBUG_MESSAGE("WXMolecule::CrystUpdate():Bond not found",5)
            mpBondWin->AppendRows();
            mvpBond.push_back(CellBond());
            mvpBond.back().mpBond=mpMolecule->GetBondList()[i];
         }
      }
      if(0!=mpAngleWin)
      {
         for(unsigned long i=mvpBondAngle.size();i<mpMolecule->GetBondAngleList().size();++i)
         {
            VFN_DEBUG_MESSAGE("WXMolecule::CrystUpdate():Bond Angle not found",5)
            mpAngleWin->AppendRows();
            mvpBondAngle.push_back(CellBondAngle());
            mvpBondAngle.back().mpBondAngle=mpMolecule->GetBondAngleList()[i];
         }
      }
      if(0!=mpDihedralAngleWin)
      {
         for(unsigned long i=mvpDihedralAngle.size();i<mpMolecule->GetDihedralAngleList().size();++i)
         {
            VFN_DEBUG_MESSAGE("WXMolecule::CrystUpdate():Dihedral Angle not found",5)
            mpDihedralAngleWin->AppendRows();
            mvpDihedralAngle.push_back(CellDihedralAngle());
            mvpDihedralAngle.back().mpDihedralAngle=mpMolecule->GetDihedralAngleList()[i];
         }
      }
      if(0!=mpRigidGroupWin)
      {
         for(unsigned long i=mvpRigidGroup.size();i<mpMolecule->GetRigidGroupList().size();++i)
         {
            VFN_DEBUG_MESSAGE("WXMolecule::CrystUpdate():Rigid Group not found",5)
            mpRigidGroupWin->AppendRows();
            mvpRigidGroup.push_back(CellRigidGroup());
            mvpRigidGroup.back().mpGroup=mpMolecule->GetRigidGroupList()[i];
         }
         // Update list of atoms, if necessary
         for(list<CellRigidGroup>::iterator pos=mvpRigidGroup.begin();pos!=mvpRigidGroup.end();++pos)
         {
            if(*(pos->mpGroup) != pos->mGroupCopy)
            {
               pos->mGroupCopy=*(pos->mpGroup);
               pos->mNeedUpdateUI=true;
            }
         }
      }
      if(0!=mpNonFlipAtomWin)
      {
          vector<MolAtom*> v = mpMolecule->GetAtomList();
          unsigned int i=0;
          for(std::vector<MolAtom*>::const_iterator pos=mpMolecule->GetAtomList().begin();pos!=mpMolecule->GetAtomList().end();++pos)
          {
              if((*pos)->IsNonFlipAtom())
              {
                 mpNonFlipAtomWin->AppendRows(1, true);
                 mpNonFlipAtomWin->SetCellValue(i, 0, (*pos)->GetName());
                 i++;
              }
          }
      }
   }
   // Update values
   {
      for(list<CellAtom>::iterator pos=mvpAtom.begin();pos!=mvpAtom.end();++pos)
      {
         const string name=pos->mpAtom->GetName();
         const ScatteringPower* pow=&(pos->mpAtom->GetScatteringPower());
         const REAL x=pos->mpAtom->X();
         const REAL y=pos->mpAtom->Y();
         const REAL z=pos->mpAtom->Z();
         const REAL occ=pos->mpAtom->GetOccupancy();
         if(  (name !=pos->mName)
            ||(pow  !=pos->mpScatteringPower)
            ||(x    !=pos->mX)
            ||(y    !=pos->mY)
            ||(z    !=pos->mZ)
            ||(occ  !=pos->mOcc))
         {
            pos->mName  =name;
            pos->mpScatteringPower  =pow;
            pos->mX =x;
            pos->mY =y;
            pos->mZ =z;
            pos->mOcc =occ;
            pos->mNeedUpdateUI=true;
         }
      }
   }
   if(0!=mpBondWin)
   {
      for(list<CellBond>::iterator pos=mvpBond.begin();pos!=mvpBond.end();++pos)
      {
         const string atom1=pos->mpBond->GetAtom1().GetName();
         const string atom2=pos->mpBond->GetAtom2().GetName();
         const REAL length  =pos->mpBond->GetLength();
         const REAL length0 =pos->mpBond->GetLength0();
         const REAL sigma   =pos->mpBond->GetLengthSigma();
         const REAL delta   =pos->mpBond->GetLengthDelta();
         if(  (atom1  !=pos->mAtom1)
            ||(atom2  !=pos->mAtom2)
            ||(length !=pos->mLength)
            ||(length0!=pos->mLength0)
            ||(sigma  !=pos->mSigma)
            ||(delta  !=pos->mDelta))
         {
            VFN_DEBUG_MESSAGE("WXMolecule::CrystUpdate():"<<atom1<<"-"<<atom2<<":"<<length,4)
            pos->mAtom1  =atom1;
            pos->mAtom2  =atom2;
            pos->mLength =length;
            pos->mLength0=length0;
            pos->mSigma  =sigma;
            pos->mDelta  =delta;
            pos->mNeedUpdateUI=true;
         }
      }
   }
   if(0!=mpAngleWin)
   {
      for(list<CellBondAngle>::iterator pos=mvpBondAngle.begin();pos!=mvpBondAngle.end();++pos)
      {
         const string atom1=pos->mpBondAngle->GetAtom1().GetName();
         const string atom2=pos->mpBondAngle->GetAtom2().GetName();
         const string atom3=pos->mpBondAngle->GetAtom3().GetName();
         const REAL angle  =pos->mpBondAngle->GetAngle();
         const REAL angle0 =pos->mpBondAngle->GetAngle0();
         const REAL sigma   =pos->mpBondAngle->GetAngleSigma();
         const REAL delta   =pos->mpBondAngle->GetAngleDelta();
         if(  (atom1 !=pos->mAtom1)
            ||(atom2 !=pos->mAtom2)
            ||(atom3 !=pos->mAtom3)
            ||(angle !=pos->mAngle)
            ||(angle0!=pos->mAngle0)
            ||(sigma !=pos->mSigma)
            ||(delta !=pos->mDelta))
         {
            pos->mAtom1 =atom1;
            pos->mAtom2 =atom2;
            pos->mAtom3 =atom3;
            pos->mAngle =angle;
            pos->mAngle0=angle0;
            pos->mSigma =sigma;
            pos->mDelta =delta;
            pos->mNeedUpdateUI=true;
         }
      }
   }
   if(0!=mpDihedralAngleWin)
   {
      for(list<CellDihedralAngle>::iterator pos=mvpDihedralAngle.begin();pos!=mvpDihedralAngle.end();++pos)
      {
         const string atom1=pos->mpDihedralAngle->GetAtom1().GetName();
         const string atom2=pos->mpDihedralAngle->GetAtom2().GetName();
         const string atom3=pos->mpDihedralAngle->GetAtom3().GetName();
         const string atom4=pos->mpDihedralAngle->GetAtom4().GetName();
         const REAL angle  =pos->mpDihedralAngle->GetAngle();
         const REAL angle0 =pos->mpDihedralAngle->GetAngle0();
         const REAL sigma   =pos->mpDihedralAngle->GetAngleSigma();
         const REAL delta   =pos->mpDihedralAngle->GetAngleDelta();
         if(  (atom1 !=pos->mAtom1)
            ||(atom2 !=pos->mAtom2)
            ||(atom3 !=pos->mAtom3)
            ||(atom4 !=pos->mAtom4)
            ||(angle !=pos->mAngle)
            ||(angle0!=pos->mAngle0)
            ||(sigma !=pos->mSigma)
            ||(delta !=pos->mDelta))
         {
            pos->mAtom1 =atom1;
            pos->mAtom2 =atom2;
            pos->mAtom3 =atom3;
            pos->mAtom4 =atom4;
            pos->mAngle =angle;
            pos->mAngle0=angle0;
            pos->mSigma =sigma;
            pos->mDelta =delta;
            pos->mNeedUpdateUI=true;
         }
      }
   }
   if(0!=mpNonFlipAtomWin)
   {

   }
   this->WXRefinableObj::CrystUpdate(uui,false);
   if(lock) mMutex.Unlock();
   VFN_DEBUG_EXIT("WXMolecule::CrystUpdate()",6)
}

void WXMolecule::OnMenuShowRestraintWindow(wxCommandEvent &event)
{
   if(0!=mpBondWin) return;

   // Frame with notebook
      wxFrame *frame= new wxFrame(this,-1,_T("Restraints for: ")+wxString::FromAscii(mpMolecule->GetName().c_str()),
                                  wxDefaultPosition,wxSize(800,300));

      wxNotebook *notebook = new wxNotebook(frame, -1);
   // Bond lengths
   {
      wxGridCellAttr* cellAttrName = new wxGridCellAttr;
      cellAttrName->SetRenderer(new wxGridCellStringRenderer);
      cellAttrName->SetEditor(new wxGridCellTextEditor);
      wxGridCellAttr* cellAttrFloat = new wxGridCellAttr;
      cellAttrFloat->SetRenderer(new wxGridCellFloatRenderer(-1,3));
      cellAttrFloat->SetEditor(new wxGridCellFloatEditor);
      wxGridCellAttr* cellAttrFloatReadOnly = new wxGridCellAttr;
      cellAttrFloatReadOnly->SetRenderer(new wxGridCellFloatRenderer(-1,3));
      cellAttrFloatReadOnly->SetEditor(new wxGridCellFloatEditor);
      cellAttrFloatReadOnly->SetReadOnly();

      mpBondWin = new WXMolScrolledWindow(notebook,this,ID_WINDOW_BONDLENGTH);
      notebook->AddPage(mpBondWin, _T("Bond Lengths"), true);
      mpBondWin->CreateGrid(0,6);
      mpBondWin->SetColSize(0,80);
      mpBondWin->SetColAttr(0,cellAttrName);
      mpBondWin->SetColAttr(1,cellAttrName->Clone());
      mpBondWin->SetColAttr(2,cellAttrFloatReadOnly);
      mpBondWin->SetColAttr(3,cellAttrFloat);
      mpBondWin->SetColAttr(4,cellAttrFloat->Clone());
      mpBondWin->SetColAttr(5,cellAttrFloat->Clone());
      mpBondWin->SetColLabelValue(0,_T("Atom1"));
      mpBondWin->SetColLabelValue(1,_T("Atom2"));
      mpBondWin->SetColLabelValue(2,_T("Length"));
      mpBondWin->SetColLabelValue(3,_T("Restraint"));
      mpBondWin->SetColLabelValue(4,_T("Sigma"));
      mpBondWin->SetColLabelValue(5,_T("Delta"));
      mpBondWin->AutoSizeRows();
   }
   // Bond angles
   {
      wxGridCellAttr* cellAttrName = new wxGridCellAttr;
      cellAttrName->SetRenderer(new wxGridCellStringRenderer);
      cellAttrName->SetEditor(new wxGridCellTextEditor);
      wxGridCellAttr* cellAttrFloat = new wxGridCellAttr;
      cellAttrFloat->SetRenderer(new wxGridCellFloatRenderer(-1,3));
      cellAttrFloat->SetEditor(new wxGridCellFloatEditor);
      wxGridCellAttr* cellAttrFloatReadOnly = new wxGridCellAttr;
      cellAttrFloatReadOnly->SetRenderer(new wxGridCellFloatRenderer(-1,3));
      cellAttrFloatReadOnly->SetEditor(new wxGridCellFloatEditor);
      cellAttrFloatReadOnly->SetReadOnly();

      mpAngleWin = new WXMolScrolledWindow(notebook,this,ID_WINDOW_BONDANGLE);
      notebook->AddPage(mpAngleWin, _T("Bond Angles"), true);
      mpAngleWin->CreateGrid(0,7);
      mpAngleWin->SetColSize(0,80);
      mpAngleWin->SetColAttr(0,cellAttrName);
      mpAngleWin->SetColAttr(1,cellAttrName->Clone());
      mpAngleWin->SetColAttr(2,cellAttrName->Clone());
      mpAngleWin->SetColAttr(3,cellAttrFloatReadOnly);
      mpAngleWin->SetColAttr(4,cellAttrFloat);
      mpAngleWin->SetColAttr(5,cellAttrFloat->Clone());
      mpAngleWin->SetColAttr(6,cellAttrFloat->Clone());
      mpAngleWin->SetColLabelValue(0,_T("Atom1"));
      mpAngleWin->SetColLabelValue(1,_T("Atom2"));
      mpAngleWin->SetColLabelValue(2,_T("Atom3"));
      mpAngleWin->SetColLabelValue(3,_T("Angle"));
      mpAngleWin->SetColLabelValue(4,_T("Restraint"));
      mpAngleWin->SetColLabelValue(5,_T("Sigma"));
      mpAngleWin->SetColLabelValue(6,_T("Delta"));
      mpAngleWin->AutoSizeRows();
   }
   // Dihedral angles
   {
      wxGridCellAttr* cellAttrName = new wxGridCellAttr;
      cellAttrName->SetRenderer(new wxGridCellStringRenderer);
      cellAttrName->SetEditor(new wxGridCellTextEditor);
      wxGridCellAttr* cellAttrFloat = new wxGridCellAttr;
      cellAttrFloat->SetRenderer(new wxGridCellFloatRenderer(-1,3));
      cellAttrFloat->SetEditor(new wxGridCellFloatEditor);
      wxGridCellAttr* cellAttrFloatReadOnly = new wxGridCellAttr;
      cellAttrFloatReadOnly->SetRenderer(new wxGridCellFloatRenderer(-1,3));
      cellAttrFloatReadOnly->SetEditor(new wxGridCellFloatEditor);
      cellAttrFloatReadOnly->SetReadOnly();

      mpDihedralAngleWin = new WXMolScrolledWindow(notebook,this,ID_WINDOW_DIHEDRALANGLE);
      notebook->AddPage(mpDihedralAngleWin, _T("Dihedral Angles"), true);
      mpDihedralAngleWin->CreateGrid(0,8);
      mpDihedralAngleWin->SetColSize(0,80);
      mpDihedralAngleWin->SetColAttr(0,cellAttrName);
      mpDihedralAngleWin->SetColAttr(1,cellAttrName->Clone());
      mpDihedralAngleWin->SetColAttr(2,cellAttrName->Clone());
      mpDihedralAngleWin->SetColAttr(3,cellAttrName->Clone());
      mpDihedralAngleWin->SetColAttr(4,cellAttrFloatReadOnly);
      mpDihedralAngleWin->SetColAttr(5,cellAttrFloat);
      mpDihedralAngleWin->SetColAttr(6,cellAttrFloat->Clone());
      mpDihedralAngleWin->SetColAttr(7,cellAttrFloat->Clone());
      mpDihedralAngleWin->SetColLabelValue(0,_T("Atom1"));
      mpDihedralAngleWin->SetColLabelValue(1,_T("Atom2"));
      mpDihedralAngleWin->SetColLabelValue(2,_T("Atom3"));
      mpDihedralAngleWin->SetColLabelValue(3,_T("Atom4"));
      mpDihedralAngleWin->SetColLabelValue(4,_T("Angle"));
      mpDihedralAngleWin->SetColLabelValue(5,_T("Restraint"));
      mpDihedralAngleWin->SetColLabelValue(6,_T("Sigma"));
      mpDihedralAngleWin->SetColLabelValue(7,_T("Delta"));
      mpDihedralAngleWin->AutoSizeRows();
   }
   // Rigid groups
   {
      wxGridCellAttr* cellAttrName = new wxGridCellAttr;
      cellAttrName->SetRenderer(new wxGridCellStringRenderer);
      cellAttrName->SetEditor(new wxGridCellTextEditor);

      mpRigidGroupWin = new WXMolScrolledWindow(notebook,this,ID_WINDOW_RIGIDGROUP);
      notebook->AddPage(mpRigidGroupWin, _T("Rigid Groups"), true);
      mpRigidGroupWin->CreateGrid(0,1);
      mpRigidGroupWin->SetColMinimalWidth(0,600);
      mpRigidGroupWin->SetColSize(0,600);
      mpRigidGroupWin->SetColAttr(0,cellAttrName);
      mpRigidGroupWin->SetColLabelValue(0,_T("Atoms in Rigid Group"));
      //mpRigidGroupWin->ForceRefresh();
      //mpRigidGroupWin->AutoSizeRows();
   }

   //non-flip atoms
   {
      wxGridCellAttr* cellAttrName = new wxGridCellAttr;
      cellAttrName->SetReadOnly();
      cellAttrName->SetRenderer(new wxGridCellStringRenderer);
      cellAttrName->SetEditor(new wxGridCellTextEditor);

      mpNonFlipAtomWin = new WXMolScrolledWindow(notebook,this,ID_WINDOW_NONFLIPATOM);
      notebook->AddPage(mpNonFlipAtomWin, _T("Optically active atoms"), true);
      mpNonFlipAtomWin->CreateGrid(0,1);
      mpNonFlipAtomWin->SetColMinimalWidth(0,600);
      mpNonFlipAtomWin->SetColSize(0,600);
      mpNonFlipAtomWin->SetColAttr(0,cellAttrName);
      mpNonFlipAtomWin->SetColLabelValue(0,_T("Optically active atom"));
   }
   notebook->SetSelection(0);
   this->CrystUpdate(true);
   frame->Show(true);
   frame->Layout();
}

void WXMolecule::OnMenuRigidfyWithDihedralAngles(wxCommandEvent & WXUNUSED(event))
{
   VFN_DEBUG_ENTRY("WXMolecule::OnMenuRigidfyWithDihedralAngles()",6)
   wxString msg;
   msg.Printf( _T("This will add all possible dihedral angles,\n")
               _T("in practice making the Molecule rigid\n\n")
               _T("Are you sure you want to proceed ?")
              );

   wxMessageDialog w(this,msg,_T("Warning !"),wxYES_NO|wxNO_DEFAULT|wxICON_EXCLAMATION);
   int result=w.ShowModal();
   if(wxID_YES==result) mpMolecule->RigidifyWithDihedralAngles();

   VFN_DEBUG_EXIT("WXMolecule::OnMenuRigidfyWithDihedralAngles()",6)
}

void WXMolecule::OnMenuSetDeltaSigma(wxCommandEvent &event)
{
   VFN_DEBUG_ENTRY("WXMolecule::OnMenuSetDeltaSigma()",6)
   WXCrystValidateAllUserInput();
   double sigma=0.01,delta=0.02;
   {
      wxString s;
      s.Printf(_T("%f"),delta);
      wxString title=_T("Choose 'delta' value");
      wxString info;
      info.Printf(_T("The 'delta' value is the allowed range \n")
                  _T("(without penalty) around the expected value.\n\n")
                  _T("It is by default equal to 0.02, in Angstroems for bond lengths,\n")
                  _T("and in radians for angles (0.02rad = 1.15deg)\n\n")
                  _T("DO NOT TRY TO CHANGE THE DEFAULT VALUE\n")
                  _T("UNLESS YOU REALLY KNOW WHAT YOU ARE DOING\n")
                  _T("Fox has been optimized with the default values...")
                 );
      wxTextEntryDialog dialog(this,info,title,s,wxOK|wxCANCEL);
      if(wxID_OK!=dialog.ShowModal())
      {
         VFN_DEBUG_EXIT("WXMolecule::OnMenuSetDeltaSigma():Canceled",6)
         return;
      }
      dialog.GetValue().ToDouble(&delta);
   }
   {
      wxString s;
      s.Printf(_T("%f"),sigma);
      wxString title=_T("Choose 'sigma' value");
      wxString info;
      info.Printf(_T("The 'sigma' value is used to compute \n")
                  _T("penalty)around the expected value\n\n")
                  _T("It is by default equal to 0.01, in Angstroems for bond angles,\n")
                  _T("and in radians for angles (0.01rad = 0.57deg)\n\n")
                  _T("DO NOT TRY TO CHANGE THE DEFAULT VALUE\n")
                  _T("UNLESS YOU REALLY KNOW WHAT YOU ARE DOING\n")
                  _T("Fox has been optimized with the default values...")
                 );
      wxTextEntryDialog dialog(this,info,title.c_str(),s,wxOK|wxCANCEL);
      if(wxID_OK!=dialog.ShowModal())
      {
         VFN_DEBUG_EXIT("WXMolecule::OnMenuSetDeltaSigma():Canceled",6)
         return;
      }
      dialog.GetValue().ToDouble(&sigma);
   }
   for(vector<MolBond*>::iterator pos=mpMolecule->GetBondList().begin();
       pos != mpMolecule->GetBondList().end();++pos)
   {
      (*pos)->SetLengthDelta(delta);
      (*pos)->SetLengthSigma(sigma);
   }
   for(vector<MolBondAngle*>::iterator pos=mpMolecule->GetBondAngleList().begin();
       pos != mpMolecule->GetBondAngleList().end();++pos)
   {
      (*pos)->SetAngleDelta(delta*DEG2RAD);
      (*pos)->SetAngleSigma(sigma*DEG2RAD);
   }
   for(vector<MolDihedralAngle*>::iterator pos=mpMolecule->GetDihedralAngleList().begin();
       pos != mpMolecule->GetDihedralAngleList().end();++pos)
   {
      //(*pos)->AngleDelta()=delta;
      //(*pos)->AngleSigma()=sigma;
   }
   mpMolecule->GetBondListClock().Click();
   this->CrystUpdate(true);
   VFN_DEBUG_EXIT("WXMolecule::OnMenuSetDeltaSigma()",6)
}

void WXMolecule::OnChangeCenterAtom(wxCommandEvent &event)
{
   WXCrystValidateAllUserInput();
   int choice;
   MolAtom *const at=WXDialogChooseFromVector(mpMolecule->GetAtomList(),
                               (wxWindow*)this,"Choose a new Atom",choice);
   if(0==at) return;
   mpMolecule->SetCenterAtom(*at);
}

void WXMolecule::NotifyDeleteListWin(WXMolScrolledWindow *win)
{
   VFN_DEBUG_ENTRY("WXMolecule::NotifyDeleteListWin()",6)
   if(win==mpBondWin)
   {
      VFN_DEBUG_MESSAGE("WXMolecule::NotifyDeleteListWin(): Bond List window",5)
      mpBondWin=0;
      mvpBond.clear();
   }
   if(win==mpAngleWin)
   {
      VFN_DEBUG_MESSAGE("WXMolecule::NotifyDeleteListWin(): Angle List window",5)
      mpAngleWin=0;
      mvpBondAngle.clear();
   }
   if(win==mpDihedralAngleWin)
   {
      VFN_DEBUG_MESSAGE("WXMolecule::NotifyDeleteListWin(): Dihedral Angle List window",5)
      mpDihedralAngleWin=0;
      mvpDihedralAngle.clear();
   }
   if(win==mpRigidGroupWin)
   {
      VFN_DEBUG_MESSAGE("WXMolecule::NotifyDeleteListWin(): Dihedral Angle List window",5)
      mpRigidGroupWin=0;
      mvpRigidGroup.clear();
   }
   if(win==mpNonFlipAtomWin)
   {
      VFN_DEBUG_MESSAGE("WXMolecule::NotifyDeleteListWin(): non-flip atom list window",5)
      mpNonFlipAtomWin=0;
   }
   VFN_DEBUG_EXIT("WXMolecule::NotifyDeleteListWin()",6)
}
void WXMolecule::UpdateUI(const bool lock)
{
   if(lock) mMutex.Lock();
   VFN_DEBUG_ENTRY("WXMolecule::UpdateUI()",5)
   {
      unsigned long i=0;
      for(list<CellAtom>::iterator pos=mvpAtom.begin();pos!=mvpAtom.end();++pos)
      {
         if(pos->mNeedUpdateUI==true)
         {
            mIsSelfUpdating=true;
            mpAtomWin->SetCellValue(i, 0, wxString::FromAscii(pos->mName.c_str()));
            mpAtomWin->SetCellValue(i, 1, wxString::FromAscii(pos->mpScatteringPower->GetName().c_str()));
            wxString tmp;
            tmp.Printf(_T("%.3f"),pos->mX);
            mpAtomWin->SetCellValue(i, 2, tmp);
            tmp.Printf(_T("%.3f"),pos->mY);
            mpAtomWin->SetCellValue(i, 3, tmp);
            tmp.Printf(_T("%.3f"),pos->mZ);
            mpAtomWin->SetCellValue(i, 4, tmp);
            tmp.Printf(_T("%.3f"),pos->mOcc);
            mpAtomWin->SetCellValue(i, 5, tmp);
            mIsSelfUpdating=false;
         }
         ++i;
      }
   }
   if(0!=mpBondWin)
   {
      unsigned long i=0;
      for(list<CellBond>::iterator pos=mvpBond.begin();pos!=mvpBond.end();++pos)
      {
         if(pos->mNeedUpdateUI==true)
         {
            mIsSelfUpdating=true;
            mpBondWin->SetCellValue(i, 0, wxString::FromAscii(pos->mAtom1.c_str()));
            mpBondWin->SetCellValue(i, 1, wxString::FromAscii(pos->mAtom2.c_str()));
            wxString tmp;
            tmp.Printf(_T("%f"),pos->mLength);
            mpBondWin->SetCellValue(i, 2, tmp);
            tmp.Printf(_T("%f"),pos->mLength0);
            mpBondWin->SetCellValue(i, 3, tmp);
            tmp.Printf(_T("%f"),pos->mSigma);
            mpBondWin->SetCellValue(i, 4, tmp);
            tmp.Printf(_T("%f"),pos->mDelta);
            mpBondWin->SetCellValue(i, 5, tmp);
            mIsSelfUpdating=false;
         }
         ++i;
      }
   }
   if(0!=mpAngleWin)
   {
      unsigned long i=0;
      for(list<CellBondAngle>::iterator pos=mvpBondAngle.begin();pos!=mvpBondAngle.end();++pos)
      {
         if(pos->mNeedUpdateUI==true)
         {
            mIsSelfUpdating=true;
            mpAngleWin->SetCellValue(i, 0, wxString::FromAscii(pos->mAtom1.c_str()));
            mpAngleWin->SetCellValue(i, 1, wxString::FromAscii(pos->mAtom2.c_str()));
            mpAngleWin->SetCellValue(i, 2, wxString::FromAscii(pos->mAtom3.c_str()));
            wxString tmp;
            tmp.Printf(_T("%f"),pos->mAngle*RAD2DEG);
            mpAngleWin->SetCellValue(i, 3, tmp);
            tmp.Printf(_T("%f"),pos->mAngle0*RAD2DEG);
            mpAngleWin->SetCellValue(i, 4, tmp);
            tmp.Printf(_T("%f"),pos->mSigma*RAD2DEG);
            mpAngleWin->SetCellValue(i, 5, tmp);
            tmp.Printf(_T("%f"),pos->mDelta*RAD2DEG);
            mpAngleWin->SetCellValue(i, 6, tmp);
            mIsSelfUpdating=false;
         }
         ++i;
      }
   }
   if(0!=mpDihedralAngleWin)
   {
      unsigned long i=0;
      for(list<CellDihedralAngle>::iterator pos=mvpDihedralAngle.begin();pos!=mvpDihedralAngle.end();++pos)
      {
         if(pos->mNeedUpdateUI==true)
         {
            mIsSelfUpdating=true;
            mpDihedralAngleWin->SetCellValue(i, 0, wxString::FromAscii(pos->mAtom1.c_str()));
            mpDihedralAngleWin->SetCellValue(i, 1, wxString::FromAscii(pos->mAtom2.c_str()));
            mpDihedralAngleWin->SetCellValue(i, 2, wxString::FromAscii(pos->mAtom3.c_str()));
            mpDihedralAngleWin->SetCellValue(i, 3, wxString::FromAscii(pos->mAtom4.c_str()));
            wxString tmp;
            tmp.Printf(_T("%f"),pos->mAngle*RAD2DEG);
            mpDihedralAngleWin->SetCellValue(i, 4, tmp);
            tmp.Printf(_T("%f"),pos->mAngle0*RAD2DEG);
            mpDihedralAngleWin->SetCellValue(i, 5, tmp);
            tmp.Printf(_T("%f"),pos->mSigma*RAD2DEG);
            mpDihedralAngleWin->SetCellValue(i, 6, tmp);
            tmp.Printf(_T("%f"),pos->mDelta*RAD2DEG);
            mpDihedralAngleWin->SetCellValue(i, 7, tmp);
            mIsSelfUpdating=false;
         }
         ++i;
      }
   }
   if(0!=mpRigidGroupWin)
   {
      unsigned long i=0;
      for(list<CellRigidGroup>::iterator pos=mvpRigidGroup.begin();pos!=mvpRigidGroup.end();++pos)
      {
         if(pos->mNeedUpdateUI==true)
         {
            mIsSelfUpdating=true;
            mpRigidGroupWin->SetCellValue(i, 0,  wxString::FromAscii(pos->mpGroup->GetName().c_str()));
            mIsSelfUpdating=false;
         }
         ++i;
      }
   }
   if(0!=mpNonFlipAtomWin)
   {
       //for(unsigned long i=0;i<
       //              mpNonFlipAtomWin->SetCellValue(v[i]->GetName(), i, 1);
   }
   if(mpMolecule->GetCenterAtom()!=0)
      mpFieldCenterAtom->SetValue(mpMolecule->GetCenterAtom()->GetName());
   else mpFieldCenterAtom->SetValue("No atom !");

   //if(mpMolecule->GetOption(3).GetChoice()==0) mpFieldCenterAtom->Enable(false);
   //else mpFieldCenterAtom->Enable(true);

   if(lock) mMutex.Unlock();
   this->WXRefinableObj::UpdateUI(lock);
   VFN_DEBUG_EXIT("WXMolecule::UpdateUI()",5)
}

bool WXMolecule::Enable(bool e)
{
   if(0!=mpAtomWin)         mpAtomWin         ->Enable(e);
   if(0!=mpBondWin)         mpBondWin         ->Enable(e);
   if(0!=mpAngleWin)        mpAngleWin        ->Enable(e);
   if(0!=mpDihedralAngleWin)mpDihedralAngleWin->Enable(e);
   if(0!=mpRigidGroupWin)   mpRigidGroupWin   ->Enable(e);
   return this->::wxWindow::Enable(e);
}
} //namespace
