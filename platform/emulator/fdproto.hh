//-----------------------------------------------------------------------------
//                         Prototypes of FD-built-ins
//-----------------------------------------------------------------------------

// fdprof.cc
OZ_C_proc_proto(BIfdReset)
OZ_C_proc_proto(BIfdDiscard)
OZ_C_proc_proto(BIfdGetNext)
OZ_C_proc_proto(BIfdPrint)
OZ_C_proc_proto(BIfdTotalAverage)

// fdrel.cc
OZ_C_proc_proto(BIfdMinimum)
OZ_C_proc_proto(BIfdMaximum)
OZ_C_proc_proto(BIfdUnion)
OZ_C_proc_proto(BIfdIntersection)
OZ_C_proc_proto(BIfdLessEqOff)
OZ_C_proc_proto(BIfdNotEqEnt)
OZ_C_proc_proto(BIfdNotEq)
OZ_C_proc_proto(BIfdNotEqOff)
OZ_C_proc_proto(BIfdNotEqOffEnt)
OZ_C_proc_proto(BIfdAllDifferent)
OZ_C_proc_proto(BIfdDistinctOffset)

// fdarith.cc
OZ_C_proc_proto(BIfdPlus)
OZ_C_proc_proto(BIfdMinus)
OZ_C_proc_proto(BIfdMult)
OZ_C_proc_proto(BIfdDiv)
OZ_C_proc_proto(BIfdDivInterval)
OZ_C_proc_proto(BIfdMod)
OZ_C_proc_proto(BIfdModInterval)
OZ_C_proc_proto(BIfdPlus_rel)
OZ_C_proc_proto(BIfdMult_rel)
OZ_C_proc_proto(BIfdSquare)
OZ_C_proc_proto(BIfdTwice)

// fdbool.cc
OZ_C_proc_proto(BIfdAnd)
OZ_C_proc_proto(BIfdOr)
OZ_C_proc_proto(BIfdNot)
OZ_C_proc_proto(BIfdXor)
OZ_C_proc_proto(BIfdEquiv)
OZ_C_proc_proto(BIfdImpl)

// fdgeneric.cc
OZ_C_proc_proto(BIfdGenLinEq)
OZ_C_proc_proto(BIfdGenNonLinEq)
OZ_C_proc_proto(BIfdGenNonLinEq1)
OZ_C_proc_proto(BIfdGenLinNotEq)
OZ_C_proc_proto(BIfdGenNonLinNotEq)
OZ_C_proc_proto(BIfdGenLinLessEq)
OZ_C_proc_proto(BIfdGenNonLinLessEq)
OZ_C_proc_proto(BIfdGenNonLinLessEq1)

// fdcount.cc
OZ_C_proc_proto(BIfdElement)
OZ_C_proc_proto(BIfdElement_body)
OZ_C_proc_proto(BIfdAtMost)
OZ_C_proc_proto(BIfdAtMost_body)
OZ_C_proc_proto(BIfdAtLeast)
OZ_C_proc_proto(BIfdAtLeast_body)
OZ_C_proc_proto(BIfdCount)
OZ_C_proc_proto(BIfdCount_body)

// fdcard.cc
OZ_C_proc_proto(BIfdGenLinEqB)
OZ_C_proc_proto(BIfdGenNonLinEqB)
OZ_C_proc_proto(BIfdGenLinNotEqB)
OZ_C_proc_proto(BIfdGenNonLinNotEqB)
OZ_C_proc_proto(BIfdGenLinLessEqB)
OZ_C_proc_proto(BIfdGenNonLinLessEqB)
OZ_C_proc_proto(BIfdInB)
OZ_C_proc_proto(BIfdNotInB)
OZ_C_proc_proto(BIfdCardBIBin)
OZ_C_proc_proto(BIfdCardNestableBI)
OZ_C_proc_proto(BIfdCardNestableBIBin)

// fdcore.cc
OZ_C_proc_proto(BIisFdVar)
OZ_C_proc_proto(BIisFdVarB)
OZ_C_proc_proto(BIfdIs)
OZ_C_proc_proto(BIgetFDLimits)
OZ_C_proc_proto(BIfdMin)
OZ_C_proc_proto(BIfdMax)
OZ_C_proc_proto(BIfdGetAsList)
OZ_C_proc_proto(BIfdGetCardinality)
OZ_C_proc_proto(BIfdNextTo)
OZ_C_proc_proto(BIfdPutLe)
OZ_C_proc_proto(BIfdPutGe)
OZ_C_proc_proto(BIfdPutList)
OZ_C_proc_proto(BIfdPutInterval)
OZ_C_proc_proto(BIfdPutNot)

// fdcd.cc
OZ_C_proc_proto(BIfdConstrDisjSetUp)
OZ_C_proc_proto(BIfdConstrDisj)
OZ_C_proc_proto(BIfdConstrDisj_body)

OZ_C_proc_proto(BIfdGenLinEqCD)
OZ_C_proc_proto(BIfdGenNonLinEqCD)
OZ_C_proc_proto(BIfdGenLinEqCD_body)
OZ_C_proc_proto(BIfdGenLinLessEqCD)
OZ_C_proc_proto(BIfdGenNonLinLessEqCD)
OZ_C_proc_proto(BIfdGenLinLessEqCD_body)
OZ_C_proc_proto(BIfdGenLinNotEqCD)
OZ_C_proc_proto(BIfdGenNonLinNotEqCD)
OZ_C_proc_proto(BIfdGenLinNotEqCD_body)

OZ_C_proc_proto(BIfdPlusCD_rel)
OZ_C_proc_proto(BIfdPlusCD_rel_body)
OZ_C_proc_proto(BIfdMultCD_rel)
OZ_C_proc_proto(BIfdMultCD_rel_body)

OZ_C_proc_proto(BIfdLessEqOffCD)
OZ_C_proc_proto(BIfdLessEqOffCD_body)
OZ_C_proc_proto(BIfdNotEqCD)
OZ_C_proc_proto(BIfdNotEqCD_body)
OZ_C_proc_proto(BIfdNotEqOffCD)
OZ_C_proc_proto(BIfdNotEqOffCD_body)

OZ_C_proc_proto(BIfdPutLeCD)
OZ_C_proc_proto(BIfdPutGeCD)
OZ_C_proc_proto(BIfdPutListCD)
OZ_C_proc_proto(BIfdPutIntervalCD)
OZ_C_proc_proto(BIfdPutNotCD)

// fdmisc.cc
OZ_C_proc_proto(BIfdCardSched)
OZ_C_proc_proto(BIfdCardSchedControl)
OZ_C_proc_proto(BIfdCDSched)
OZ_C_proc_proto(BIfdCDSchedControl)
OZ_C_proc_proto(BIfdNoOverlap)
OZ_C_proc_proto(BIfdCardBIKill)
OZ_C_proc_proto(BIfdInKillB)
OZ_C_proc_proto(BIfdCardBIKill)
OZ_C_proc_proto(BIfdInKillB)
OZ_C_proc_proto(BIfdNotInKillB)
OZ_C_proc_proto(BIfdGenLinEqKillB)
OZ_C_proc_proto(BIfdGenLinLessEqKillB)
OZ_C_proc_proto(BIfdCopyDomain)
OZ_C_proc_proto(BIfdDivIntervalCons)
OZ_C_proc_proto(BIfdDivIntervalCons_body)
OZ_C_proc_proto(BIgetCopyStat)

// fdwatch.cc
OZ_C_proc_proto(BIfdWatchDom1)
OZ_C_proc_proto(BIfdWatchDom2)
OZ_C_proc_proto(BIfdWatchDom3)
OZ_C_proc_proto(BIfdWatchBounds1)
OZ_C_proc_proto(BIfdWatchBounds2)
OZ_C_proc_proto(BIfdWatchBounds3)

// body prototypes
OZ_C_proc_proto(BIfdPlus_body)
OZ_C_proc_proto(BIfdTwice_body)
OZ_C_proc_proto(BIfdMinus_body)
OZ_C_proc_proto(BIfdMult_body)
OZ_C_proc_proto(BIfdSquare_body)
OZ_C_proc_proto(BIfdDiv_body)
OZ_C_proc_proto(BIfdDivInterval_body)
OZ_C_proc_proto(BIfdMod_body)
OZ_C_proc_proto(BIfdModInterval_body)
OZ_C_proc_proto(BIfdAnd_body)
OZ_C_proc_proto(BIfdOr_body)
OZ_C_proc_proto(BIfdNot_body)
OZ_C_proc_proto(BIfdXor_body)
OZ_C_proc_proto(BIfdEquiv_body)
OZ_C_proc_proto(BIfdImpl_body)
OZ_C_proc_proto(BIfdGenLinEq_body)
OZ_C_proc_proto(BIfdGenNonLinEq_body)
OZ_C_proc_proto(BIfdGenLinNotEq_body)
OZ_C_proc_proto(BIfdGenNonLinNotEq_body)
OZ_C_proc_proto(BIfdGenLinLessEq_body)
OZ_C_proc_proto(BIfdGenNonLinLessEq_body)
OZ_C_proc_proto(BIfdGenLinNotEq_body)
OZ_C_proc_proto(BIfdGenLinLessEq_body)
OZ_C_proc_proto(BIfdLessEqOff_body)
OZ_C_proc_proto(BIfdNotEqOffEnt_body)
OZ_C_proc_proto(BIfdCardSched_body)
OZ_C_proc_proto(BIfdCardSchedControl_body)
OZ_C_proc_proto(BIfdCDSched_body)
OZ_C_proc_proto(BIfdCDSchedControl_body)
OZ_C_proc_proto(BIfdDisjunction)
OZ_C_proc_proto(BIfdDisjunction_body)
OZ_C_proc_proto(BIfdNoOverlap_body)
OZ_C_proc_proto(BIfdAllDifferent_body)
OZ_C_proc_proto(BIfdDistinctOffset_body)
OZ_C_proc_proto(BIfdLessEqOff_body)
OZ_C_proc_proto(BIfdNotEqEnt_body)
OZ_C_proc_proto(BIfdNotEq_body)
OZ_C_proc_proto(BIfdNotEqOffEnt_body)
OZ_C_proc_proto(BIfdNotEqOff_body)
OZ_C_proc_proto(BIfdMaximum_body)
OZ_C_proc_proto(BIfdMinimum_body)
OZ_C_proc_proto(BIfdSubsume_body)
OZ_C_proc_proto(BIfdUnion_body)
OZ_C_proc_proto(BIfdIntersection_body)
OZ_C_proc_proto(BIfdGenLinEqB_body)
OZ_C_proc_proto(BIfdGenNonLinEqB_body)
OZ_C_proc_proto(BIfdGenLinNotEqB_body)
OZ_C_proc_proto(BIfdGenNonLinNotEqB_body)
OZ_C_proc_proto(BIfdGenLinLessEqB_body)
OZ_C_proc_proto(BIfdGenLinLessEqCD_body)
OZ_C_proc_proto(BIfdGenNonLinLessEqB_body)
OZ_C_proc_proto(BIfdGenNonLinLessEqCD_body)
OZ_C_proc_proto(BIfdCardBIKill_body)
OZ_C_proc_proto(BIfdCardBIBin_body)
OZ_C_proc_proto(BIfdInB_body)
OZ_C_proc_proto(BIfdInKillB_body)
OZ_C_proc_proto(BIfdNotInB_body)
OZ_C_proc_proto(BIfdCardNestableBI_body)
OZ_C_proc_proto(BIfdCardBIKill_body)
OZ_C_proc_proto(BIfdCardNestableBIBin_body)
OZ_C_proc_proto(BIfdInKillB_body)
OZ_C_proc_proto(BIfdNotInKillB_body)
OZ_C_proc_proto(BIfdGenLinEqKillB_body)
OZ_C_proc_proto(BIfdGenLinLessEqKillB_body)
