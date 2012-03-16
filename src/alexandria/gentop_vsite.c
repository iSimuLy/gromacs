/* -*- mode: c; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup"; -*-
 * $Id: poldata.c,v 1.20 2009/05/17 13:56:55 spoel Exp $
 * 
 *                This source code is part of
 * 
 *                 G   R   O   M   A   C   S
 * 
 *          GROningen MAchine for Chemical Simulations
 * 
 *                        VERSION 4.0.99
 * Written by David van der Spoel, Erik Lindahl, Berk Hess, and others.
 * Copyright (c) 1991-2000, University of Groningen, The Netherlands.
 * Copyright (c) 2001-2008, The GROMACS development team,
 * check out http://www.gromacs.org for more information.

 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * If you want to redistribute modifications, please consider that
 * scientific software is very special. Version control is crucial -
 * bugs must be traceable. We will be happy to consider code for
 * inclusion in the official distribution, but derived work must not
 * be called official GROMACS. Details are found in the README & COPYING
 * files - if they are missing, get the official version at www.gromacs.org.
 * 
 * To help us fund GROMACS development, we humbly ask that you cite
 * the papers on the package - you can find them in the top README file.
 * 
 * For more info, check our website at http://www.gromacs.org
 * 
 * And Hey:
 * Groningen Machine for Chemical Simulation
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "network.h"
#include "string2.h"
#include "symtab.h"
#include "vec.h"
#include "smalloc.h"
#include "gmx_fatal.h"
#include "toputil.h"
#include "gentop_vsite.h"

typedef struct {
    int nline; /* Must be 3 or 4 */
    int a[4];
} gv_linear;
	
typedef struct {
    int a[4];
    int nb[4];
} gv_planar;
	
typedef struct gentop_vsite 
{
    int egvt;
    int nlinear;
    gv_linear *lin;
    int nplanar;
    gv_planar *plan;
} gentop_vsite;

gentop_vsite_t gentop_vsite_init(int egvt)
{
    gentop_vsite_t gvt;
    
    snew(gvt,1);
    gvt->egvt = egvt;
    if ((egvt < egvtNO) || (egvt >= egvtNR))
        gvt->egvt = egvtNO;
        
    return gvt;
}

void gentop_vsite_done(gentop_vsite_t *gvt)
{
    sfree((*gvt)->lin);
    sfree((*gvt)->plan);
    sfree(*gvt);
    *gvt = NULL;
}

void gentop_vsite_add_linear(gentop_vsite_t gvt,int ai,int aj,int ak)
{
    int i;
  
    for(i=0; (i<gvt->nlinear); i++)
    {
        if (gvt->lin[i].AJ == aj) 
        {
            if (((gvt->lin[i].AI == ai)  && (gvt->lin[i].AK == ak)) ||
                ((gvt->lin[i].AI == aj)  && (gvt->lin[i].AK == ai)))
                break;
        }
    }
    if (i == gvt->nlinear)
    {
        srenew(gvt->lin,++gvt->nlinear);
        gvt->lin[i].AI = ai;
        gvt->lin[i].AJ = aj;
        gvt->lin[i].AK = ak;
    }
}

void gentop_vsite_add_planar(gentop_vsite_t gvt,int ai,int aj,int ak,int al,int nbonds[])
{
    int i,j;
  
    for(i=0; (i<gvt->nplanar); i++)
    {
        if (((gvt->plan[i].AI == ai) && (gvt->plan[i].AJ == aj)  &&
             (gvt->plan[i].AK == ak) && (gvt->plan[i].AL == al)) ||
            ((gvt->plan[i].AI == al) && (gvt->plan[i].AJ == ak)  &&
             (gvt->plan[i].AK == aj) && (gvt->plan[i].AL == ai)))
            break;
    }
    if (i == gvt->nplanar)
    {
        srenew(gvt->plan,++gvt->nplanar);
        gvt->plan[i].AI = ai;
        gvt->plan[i].AJ = aj;
        gvt->plan[i].AK = ak;
        gvt->plan[i].AL = al;
        for(j=0; (j<4); j++)
            gvt->plan[i].nb[j] = nbonds[gvt->plan[i].a[j]];
    }
}

static void delete_params(t_params plist[],int etype,int alist[])
{
    int j,k,l;
    
    switch (etype)
    {
    case F_BONDS:
        /* Remove bonds, if present */
        for(j=0; (j<plist[etype].nr); j++)
        {
            if (((plist[etype].param[j].AI == alist[0]) &&
                 (plist[etype].param[j].AJ == alist[1])) ||
                ((plist[etype].param[j].AJ == alist[0]) &&
                 (plist[etype].param[j].AI == alist[1])))
            {
                if (NULL != debug)
                    fprintf(debug,"Removing bond beteen atoms %d %d\n",
                            alist[0],alist[1]);
                for(k=j+1; (k<plist[etype].nr); k++)
                {
                    for(l=0; (l<MAXATOMLIST); l++)
                        plist[etype].param[k-1].a[l] = 
                            plist[etype].param[k].a[l];
                    for(l=0; (l<MAXFORCEPARAM); l++)
                        plist[etype].param[k-1].c[l] = 
                            plist[etype].param[k].c[l];
                }
                plist[etype].nr--;
                j--;
                break;
            }
        }
        break;
    case F_RBDIHS:
    case F_PDIHS:
        /* Remove dihedral, if present. Allow wildcard in alist[3] (specified as -1) */
        for(j=0; (j<plist[etype].nr); j++)
        {
            if (((plist[etype].param[j].AI == alist[0]) &&
                 (plist[etype].param[j].AJ == alist[1]) &&
                 (plist[etype].param[j].AK == alist[2]) &&
                 ((alist[3] == -1) || (plist[etype].param[j].AL == alist[3]))) ||
                ((plist[etype].param[j].AL == alist[0]) &&
                 (plist[etype].param[j].AK == alist[1]) &&
                 (plist[etype].param[j].AJ == alist[2]) &&
                 ((alist[3] == -1) || (plist[etype].param[j].AI == alist[3]))) ||
                ((plist[etype].param[j].AJ == alist[0]) &&
                 (plist[etype].param[j].AK == alist[1]) &&
                 (plist[etype].param[j].AL == alist[2]) &&
                 (alist[3] == -1)))
            {
                if (NULL != debug)
                    fprintf(debug,"Removing dihedral beteen atoms %d %d %d %d\n",
                            alist[0],alist[1],alist[2],alist[3]);
                for(k=j+1; (k<plist[etype].nr); k++)
                {
                    for(l=0; (l<MAXATOMLIST); l++)
                        plist[etype].param[k-1].a[l] = 
                            plist[etype].param[k].a[l];
                    for(l=0; (l<MAXFORCEPARAM); l++)
                        plist[etype].param[k-1].c[l] = 
                            plist[etype].param[k].c[l];
                }
                plist[etype].nr--;
                j--;
            }
        }
        break;
    case F_ANGLES:
        /* Remove angle, if present */
        for(j=0; (j<plist[etype].nr); j++)
        {
            if (plist[etype].param[j].AJ == alist[1])
            {
                if (((plist[etype].param[j].AI == alist[0]) &&
                     (plist[etype].param[j].AK == alist[2])) ||
                    ((plist[etype].param[j].AK == alist[0]) &&
                     (plist[etype].param[j].AI == alist[2])))
                {
                    if (NULL != debug)
                        fprintf(debug,"Removing angle beteen atoms %d %d %d\n",
                                alist[0],alist[1],alist[2]);
                    for(k=j+1; (k<plist[etype].nr); k++)
                    {
                        for(l=0; (l<MAXATOMLIST); l++)
                            plist[etype].param[k-1].a[l] = 
                                plist[etype].param[k].a[l];
                        for(l=0; (l<MAXFORCEPARAM); l++)
                            plist[etype].param[k-1].c[l] = 
                                plist[etype].param[k].c[l];
                    }
                    plist[etype].nr--;
                    j--;
                    break;
                }
            }
        }
        break;
    default:
        fprintf(stderr,"Don't know how to remove params from type %d\n",etype);
    }
}

static void calc_vsite2parm(t_atoms *atoms,t_params plist[],rvec **x,
                            gv_linear *gvl,t_symtab *symtab,
                            gpp_atomtype_t atype)
{
    int     i,j,natoms,mt;
    const   char    *ml = "ML";
    double  mI,mJ,mK,mL,mT,com,I;
    double  rB,rC,rD,rVV,mV,ac[4];
    rvec    dx,mcom;
    t_param pp,nbml;
    t_atom  aml;

    if (gvl->nline <= 0)
        return;
    memset(&nbml,0,sizeof(nbml));        
    rvec_sub((*x)[gvl->AI],(*x)[gvl->AJ],dx);
    rB    = norm(dx);
    rvec_sub((*x)[gvl->AJ],(*x)[gvl->AK],dx);
    rC    = rB+norm(dx);
    mI    = atoms->atom[gvl->AI].m;
    mJ    = atoms->atom[gvl->AJ].m;
    mK    = atoms->atom[gvl->AK].m;
    if (gvl->nline == 4) 
    {
        rvec_sub((*x)[gvl->AK],(*x)[gvl->AL],dx);
        rD = rC+norm(dx);
        mL = atoms->atom[gvl->AL].m;
    }
    else 
    {
        mL = 0;
        rD = 0;
    }
    mT    = mI+mJ+mK+mL;
	/* We need to keep the COM at the same position and the moment of inertia.
	 * In order to do this we have two variables, the position of the dummy
	 * and the relative masses (we also need to keep the total mass constant).
	 * The atom I should be the one connecting to the remainder of the molecule.
	 * We put the first atom I at coordinate 0.
	 */
    com   = (mJ*rB+mK*rC+mL*rD)/(mT);
    I     = mI*sqr(com) + mJ*sqr(rB-com) + mK*sqr(rC-com) + mL*sqr(rD-com);
    rVV   = com+I/(com*mT);
	mV    = com*mT/rVV;
    if (NULL != debug)
        fprintf(debug,"com = %g, I = %g, rVV = %g mV = %g rB = %g rC = %g rD = %g\n",
                com,I,rVV,mV,rB,rC,rD);
	mI    = (mJ+mK+mL-mV);
	if (mI <= 0)
		gmx_fatal(FARGS,"Zero or negative mass %f in virtual site construction",mI);
    ac[0] = 0;
    ac[1] = (rB/rVV);
    ac[2] = (rC/rVV);
    ac[3] = (rD/rVV);
    
    natoms = atoms->nr;
    add_t_atoms(atoms,1,0);
    srenew(*x,natoms+1);
   
    /* Add coordinates for mass-particles */
    clear_rvec((*x)[natoms]);
    for(i=1; (i<3); i++)
        for(j=0; (j<DIM); j++)
            (*x)[natoms][j] += (atoms->atom[gvl->a[i]].m/mV)*(*x)[gvl->a[i]][j];
    /* Update t_atoms for atoms that change/lose their masses */
    atoms->atom[gvl->a[0]].m  = mI;
    atoms->atom[gvl->a[0]].mB = mI;
    for(i=1; (i<3); i++)
    {
        atoms->atom[gvl->a[i]].m  = 0;
        atoms->atom[gvl->a[i]].mB = 0;
    }  
    /* Set information in atoms structure for the new mass-particles */
    memset(&aml,0,sizeof(aml));
    mt = add_atomtype(atype,symtab,&aml,ml,&nbml,NOTSET,0,0,0,0,0,0);
    for(i=natoms; (i<=natoms); i++)
    {
        atoms->atom[i].m          = mV;
        atoms->atom[i].mB         = mV;
        atoms->atom[i].atomnumber = 0;
        if (NULL != atoms->atomname)
            atoms->atomname[i]        = put_symtab(symtab,ml);
        if (NULL != atoms->atomtype)
            atoms->atomtype[i]        = put_symtab(symtab,ml);
        if (NULL != atoms->atomtypeB)
            atoms->atomtypeB[i]       = put_symtab(symtab,ml);
        atoms->atom[i].type       = mt;
        atoms->atom[i].typeB      = mt;
    }
    
    /* Add constraint between masses */
    memset(&pp,0,sizeof(pp));
    pp.AI = gvl->AI;
    pp.AJ = natoms;
    pp.C0 = rVV;
    add_param_to_list(&(plist[F_CONSTR]), &pp);
    
    /* Add vsites */
    for(i=1; (i<gvl->nline); i++)
    {
        memset(&pp,0,sizeof(pp));
        pp.AI = gvl->a[i];
        pp.AJ = gvl->AI;
        pp.AK = natoms;
        pp.C0 = ac[i];
        add_param_to_list(&(plist[F_VSITE2]), &pp);
    }
}

void gentop_vsite_check(gentop_vsite_t gvt,int natom)
{
    int i,j,k,l,ai,aj,ndbl,found;
    
    for(i=0; (i<gvt->nlinear); i++)
        gvt->lin[i].nline = 3;
    
    for(i=0; (i<gvt->nlinear); i++)
    {
        for(j=i+1; (j<gvt->nlinear); j++)
        {
            ndbl = 0;
            for(k=0; (k<gvt->lin[i].nline); k++) 
            {
                ai = gvt->lin[i].a[k];
                for(l=0; (l<gvt->lin[j].nline); l++) 
                {
                    aj = gvt->lin[j].a[l];
                    if (ai == aj)
                    {
                        ndbl++;
                    }
                }
            }
            
            ndbl /= 2;
            if (ndbl > 0) 
            {
                fprintf(stderr,"WARNING: merging two linear vsites into one. Please check result.\n");
                if (NULL != debug)
                {
                    fprintf(debug,"Linear group j");
                    for(l=0; (l<gvt->lin[j].nline); l++) 
                        fprintf(debug," %d",gvt->lin[j].a[l]);
                    fprintf(debug,"\n");
                    fprintf(debug,"Linear group i");
                    for(k=0; (k<gvt->lin[i].nline); k++) 
                        fprintf(debug," %d",gvt->lin[i].a[k]);
                    fprintf(debug,"\n");
                }
                if ((gvt->lin[j].AI == gvt->lin[i].AJ) && 
                    (gvt->lin[j].AJ == gvt->lin[i].AK)) 
                {
                    gvt->lin[i].AL = gvt->lin[j].AK;
                    gvt->lin[i].nline = 4;
                    gvt->lin[j].nline = 0;
                }
                else if ((gvt->lin[i].AI == gvt->lin[j].AJ) && 
                         (gvt->lin[i].AJ == gvt->lin[j].AK)) 
                {
                    gvt->lin[j].AL = gvt->lin[i].AK;
                    gvt->lin[j].nline = 4;
                    gvt->lin[i].nline = 0;
                }
                else
                {
                    gmx_fatal(FARGS,"Atoms in strange order in linear vsites. Check debug file.");
                }
                if (NULL != debug)
                {
                    fprintf(debug,"Linear group j");
                    for(l=0; (l<gvt->lin[j].nline); l++) 
                        fprintf(debug," %d",gvt->lin[j].a[l]);
                    fprintf(debug,"\n");
                    fprintf(debug,"Linear group i");
                    for(k=0; (k<gvt->lin[i].nline); k++) 
                        fprintf(debug," %d",gvt->lin[i].a[k]);
                    fprintf(debug,"\n");
                }
            }
        }
    }
}

void gentop_vsite_generate_special(gentop_vsite_t gvt,gmx_bool bGenVsites,
                                   t_atoms *atoms,rvec **x,
                                   t_params plist[],t_symtab *symtab,
                                   gpp_atomtype_t atype,t_excls **excls)
{
    int     i,j,k,l,natoms_old,nlin_at;
    int     a[MAXATOMLIST],aa[2];
    t_param pp;
    
    gentop_vsite_check(gvt,atoms->nr);
    nlin_at = 0;
    for(i=0; (i<gvt->nlinear); i++) 
        nlin_at += gvt->lin[i].nline;
        
    printf("Generating %d linear %s and %d impropers\n",
           nlin_at,(bGenVsites ? "vsites" : "angles"),
           gvt->nplanar);
    if ((gvt->egvt == egvtLINEAR) || (gvt->egvt == egvtALL))
    {
        /* If we use vsites (discouraged) each triplet of atoms in a linear arrangement 
         * is described by
         * two dummy masses connected by a constraint, that maintain the 
         * moment of inertia. The three atoms are then generated from these two 
         * positions using a virtual_site2 construction. We need to add 2 extra
         * particles for each linear group.
         * In case we use the special linear angle terms, life gets a lot easier!
         */
        for(i=0; (i<gvt->nlinear); i++) 
        {
            for(j=0; (j<gvt->lin[i].nline); j++)
                a[j] = gvt->lin[i].a[j];
            for( ; (j<MAXATOMLIST); j++)
                a[j] = -1;
            delete_params(plist,F_ANGLES,a);
            delete_params(plist,F_RBDIHS,a);
            delete_params(plist,F_PDIHS,a);
            
            if (bGenVsites)
            {
                /* Complicated algorithm, watch out */
                for(j=0; (j<gvt->lin[i].nline-1); j++)
                {
                    aa[0] = a[j+0];
                    aa[1] = a[j+1];
                    delete_params(plist,F_BONDS,aa);
                }
                
                /* Compute details for the new masses and vsites, and update everything */
                calc_vsite2parm(atoms,plist,x,&gvt->lin[i],symtab,atype);
                srenew((*excls),atoms->nr);
                for(j=atoms->nr-2; (j<=atoms->nr-1); j++)
                {
                    (*excls)[j].nr = 1;
                    snew((*excls)[j].e,1);
                    (*excls)[j].e[0] = j;
                }
            }
            else
            {
                /* Linear angles */
                memset(&pp,0,sizeof(pp));
                for(j=0; (j<3); j++)
                {
                    pp.a[j] = a[j];
                }
                gmx_fatal(FARGS,"Want to add LINEAR ANGLES here");
                /*add_param_to_list(&(plist[F_LINEAR_ANGLES]),&pp);*/
            }
        }
    }
    if ((gvt->egvt == egvtPLANAR) || (gvt->egvt == egvtALL))
    {
        for(i=0; (i<gvt->nplanar); i++) 
        {
            /* First delete superfluous dihedrals */
            a[1] = gvt->plan[i].a[0]; 
            a[3] = -1;
            for(j=1; (j<4); j++) 
            {
                if (gvt->plan[i].nb[j] == 1) 
                {
                    if (j == 1) 
                    {
                        a[0] = gvt->plan[i].a[1];
                        a[2] = gvt->plan[i].a[2]; 
                        delete_params(plist,F_RBDIHS,a);
                        delete_params(plist,F_PDIHS,a);
                        a[2] = gvt->plan[i].a[3]; 
                        delete_params(plist,F_RBDIHS,a);
                        delete_params(plist,F_PDIHS,a);
                    }
                    else if (j == 2)
                    {
                        a[0] = gvt->plan[i].a[2];
                        a[2] = gvt->plan[i].a[1]; 
                        delete_params(plist,F_RBDIHS,a);
                        delete_params(plist,F_PDIHS,a);
                        a[2] = gvt->plan[i].a[3]; 
                        delete_params(plist,F_RBDIHS,a);
                        delete_params(plist,F_PDIHS,a);
                    }
                    else if (j == 3)
                    {
                        a[0] = gvt->plan[i].a[3];
                        a[2] = gvt->plan[i].a[1]; 
                        delete_params(plist,F_RBDIHS,a);
                        delete_params(plist,F_PDIHS,a);
                        a[2] = gvt->plan[i].a[2]; 
                        delete_params(plist,F_RBDIHS,a);
                        delete_params(plist,F_PDIHS,a);
                    }
                }
            }
            /* Now add impropers! */
            memset(&pp,0,sizeof(pp));
            for(j=0; (j<4); j++)
            {
                pp.a[j] = gvt->plan[i].a[j];
            }
            add_param_to_list(&(plist[F_IDIHS]),&pp);
        }
    }
}

