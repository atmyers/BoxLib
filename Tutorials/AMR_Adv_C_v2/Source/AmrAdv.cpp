
#include <ParallelDescriptor.H>
#include <ParmParse.H>
#include <MultiFabUtil.H>
#include <FillPatchUtil.H>

#include <AmrAdv.H>
#include <AmrAdvBC.H>

AmrAdv::AmrAdv ()
{
    ReadParameters();

    // Geometry on all levels has been defined already.

    // No valid BoxArray and DistributionMapping have been defined.
    // But the arrays for them have been resized.

    int nlevs_max = maxLevel() + 1;

    istep.resize(nlevs_max, 0);
    isubstep.resize(nlevs_max, 0);
    nsubsteps.resize(nlevs_max, 1);
    for (int lev = 1; lev <= maxLevel(); ++lev) {
	nsubsteps[lev] = MaxRefRatio(lev-1);
    }

    t_new.resize(nlevs_max, 0.0);
    t_old.resize(nlevs_max, -1.e100);
    dt.resize(nlevs_max, 1.e100);

    phi_new.resize(nlevs_max);
    phi_old.resize(nlevs_max);
}

AmrAdv::~AmrAdv ()
{

}

void
AmrAdv::ReadParameters ()
{
    {
	ParmParse pp;  // Traditionally, max_step and stop_time do not have prefix.
	pp.query("max_step", max_step);
	pp.query("stop_time", stop_time);
    }


    {
	ParmParse pp("adv");
	
	pp.query("cfl", cfl);
	
	pp.query("regrid_int", regrid_int);
	
	pp.query("restart", restart_chkfile);

	pp.query("check_file", check_file);
	pp.query("plot_file", plot_file);
	pp.query("check_int", check_int);
	pp.query("plot_int", plot_int);
    }
}

void
AmrAdv::AverageDown ()
{
    for (int lev = finest_level-1; lev >= 0; --lev)
    {
	BoxLib::average_down(*phi_new[lev+1], *phi_new[lev],
			     geom[lev+1], geom[lev],
			     0, phi_new[lev]->nComp(), refRatio(lev));
    }
}

long
AmrAdv::CountCells (int lev)
{
    const int N = grids[lev].size();

    long cnt = 0;

#ifdef _OPENMP
#pragma omp parallel for reduction(+:cnt)
#endif
    for (int i = 0; i < N; ++i)
    {
        cnt += grids[lev][i].numPts();
    }

    return cnt;
}

void
AmrAdv::FillPatch (int lev, Real time, MultiFab& Sborder,int icomp, int ncomp)
{
    if (lev == 0)
    {
	PArray<MultiFab> smf;
	std::vector<Real> stime;
	GetData(0, time, smf, stime);

	AmrAdvPhysBC physbc;
	BoxLib::FillPatchSingleLevel(Sborder, time, smf, stime, 0, icomp, ncomp,
				     geom[lev], physbc);
    }
    else
    {
	PArray<MultiFab> cmf, fmf;
	std::vector<Real> ctime, ftime;
	GetData(lev-1, time, cmf, ctime);
	GetData(lev  , time, fmf, ftime);

	AmrAdvPhysBC cphysbc, fphysbc;
	Array<BCRec> bcs(6);
	Interpolater* mapper = &cell_cons_interp;

	BoxLib::FillPatchTwoLevels(Sborder, time, cmf, ctime, fmf, ftime,
				   0, icomp, ncomp, geom[lev-1], geom[lev],
				   cphysbc, fphysbc, refRatio(lev-1),
				   mapper, bcs);
    }
}

void
AmrAdv::GetData (int lev, Real time, PArray<MultiFab>& data, std::vector<Real>& datatime)
{
    data.clear();
    datatime.clear();

    const Real teps = (t_new[lev] - t_old[lev]) * 1.e-3;

    if (time > t_new[lev] - teps && time < t_new[lev] + teps)
    {
	data.push_back(phi_new[lev].get());
	datatime.push_back(t_new[lev]);
    }
    else if (time > t_old[lev] - teps && time < t_old[lev] + teps)
    {
	data.push_back(phi_old[lev].get());
	datatime.push_back(t_old[lev]);
    }
    else
    {
	data.push_back(phi_old[lev].get());
	data.push_back(phi_new[lev].get());
	datatime.push_back(t_old[lev]);
	datatime.push_back(t_new[lev]);
    }
}