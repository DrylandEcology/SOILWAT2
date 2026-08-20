# SOILWAT2 Notes

[SOILWAT2]: https://github.com/DrylandEcology/SOILWAT2

Note: this document is best viewed as part of the doxygen-built documentation
(there may be text artifacts if viewed as standalone-markdown).

<br>
\section replicability Replicability of SOILWAT2 simulations

SOILWAT2 is a deterministic model, and one might expect exactly replicated
output for identical inputs. In fact, we observe bit-for-bit reproducibility of
SOILWAT2 if replicated on the same machine with the same compiler, compiler
flags, and software stack. However, simulation results are not always exactly
reproducible on different machines, different compilers, different compiler
flags, and/or differences in the software stack while the simulation itself is
not wrong. This is a well-documented phenomenon of simulation models
(Baker et al., 2015 \cite baker2015GMD;
Rosinski & Williamson, 1997 \cite rosinski1997SJSC;
Zeman & Schär, 2022 \cite zeman2022GMD).

Such discrepancies occur because floating-point arithmetic is non-associative
and because the order of floating-point operations can vary between machines,
compilers, etc.
(Goldberg, 1991 \cite goldberg1991ACS; Higham, 2002 \cite higham2002).
The small differences at the machine-rounding level do not necessarily remain
bounded particularly in long-running time-stepping simulation models.
These differences may trigger a threshold/branch divergence in the simulation
model after which the runs follow different code paths (Example 1 below).
These differences may also grow over time because of the model’s dynamic
instabilities, e.g., feedback loops in the model logic (Examples 2 and 3 below).

We have a few simulation projects for which floating-point discrepancies have
accumulated over simulation time; however, we have noticed this behavior so far
only in a handful of runs (out of 1e4 to 1e6 total runs of a project).

1. Snow age functionality triggered branch divergence. We were able to
   reduce the occurrence of branch divergence in this case with
   commit [cb186768](https://github.com/drylandEcology/SOILWAT2/commit/cb186768)
   "Fix floating point problems related to `snow_age`" (2026-May-20)

2. Dynamic surface albedo created a feedback loop. Albedo influences
   potential evapotranspiration which influences soil moisture and snowpack;
   soil moisture and snowpack in turn affect albedo.
   We observed a build-up of discrepancies across the simulation output that
   originated from surface albedo when comparing v8.4.0 and a development
   stage of v8.5.0, despite the albedo code itself being unchanged between
   versions.

3. Soil temperature is estimated iteratively, which can lead to
   instabilities. We observed a build-up of discrepancies across the
   simulation output that originated from soil temperature when hot
   restarting from a cached state, even though cached values are stored/read
   at full double precision (development of v8.5.0).

Baker et al. (2015) \cite baker2015GMD and
Zeman & Schär (2022) \cite zeman2022GMD
developed approaches to distinguish discrepancies arising from
floating-point arithmetic from those arising from meaningful code differences.

<hr>

Go back to the [main page](README.md) or
[user guide](doc/additional_pages/A_SOILWAT2_user_guide.md).
