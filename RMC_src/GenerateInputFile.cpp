//// 生成输入文件功能:后缀.FMTinp(.step0)
//// 可以生成燃耗计算中间输入文件，可直接运行
//// 由输出控制卡控制，关键字inpfile， 默认关闭
//// 暂不支持源收敛、固定源和画图计算
# include "Output.h"
# include "Geometry.h"
# include "Material.h"
# include "AceData.h"
# include "Criticality.h"
# include "Tally.h"
# include "Convergence.h"
# include "Burnup.h"
# include "Plot.h"
# include "FixedSource.h"

template<typename type>	static void PrintVaryVec(FILE *fptr,int nHead, int nSize, vector<type> &vT,char *pFmt,string cCardStr);
string CvtInitSrcType(int intsurf);
string CvtSurfType(int intsurf);

void CDOutput::GenerateInpFile(CDGeometry &cGeometry,CDMaterial &cMaterial,CDAceData &cAceData,CDCriticality &cCriticality,
	CDTally &cTally,CDConvergence &cConvergence,CDBurnup &cBurnup, CDFixedSource &cFixedSource)
{
# ifdef _IS_PARALLEL_
	if(!OParallel.p_bIsMaster)
	{
		return;
	}
# endif

	if (!p_bIsInputFilePrint)
	{
		return;
	}

	// do NOT output before corrector step (after predictor step)
	if (cBurnup.p_bIsWithBurnup && cBurnup.p_nBurnStepStrategy < 0)
	{
		return;
	}

	char p_chFormatedInpFileName[FILE_NAME_LENGTH];
	FILE *fpNewInpFilePtr;
	strcpy(p_chFormatedInpFileName,p_chInputFileName);
	strcat(p_chFormatedInpFileName,".FMTinp");

	if (cBurnup.p_bIsWithBurnup)
	{
		char chStep[10];
		sprintf(chStep,"%d",cBurnup.p_nBurnStep);
		strcat(p_chFormatedInpFileName,".step");
		strcat(p_chFormatedInpFileName,chStep);
	}
	//strcat(p_chFormatedInpFileName,".sinp");
	fpNewInpFilePtr = fopen(p_chFormatedInpFileName,"w"); 
	
	char WallClockStr[64];
	auto WallClock = time(0);         ////get system time at ending(s)
	strftime(WallClockStr, sizeof(WallClockStr), "%Y/%m/%d %X %A",localtime(&WallClock) ); 
	fprintf(fpNewInpFilePtr,"## Formatted input file generated by RMC. %s\n", WallClockStr);
	fprintf(fpNewInpFilePtr,"## Origin input: %s\n",p_chInputFileName);
	if (cBurnup.p_bIsWithBurnup)fprintf(fpNewInpFilePtr,"## Burnup step: %-4d\n",cBurnup.p_nBurnStep);

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////  1. Print UNIVERSE  ////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	{
		///////////////////////////////////// Print Cells of every Universe /////////////////////////////////////
		int nTotUnivNum = cGeometry.p_vUniverse.size();
		for(int i = 0 ; i < nTotUnivNum ; ++i )
		{
			int nUniverse_index_u=cGeometry.p_OUniverseIndex.p_vIndexU[i];
			fprintf(fpNewInpFilePtr,"UNIVERSE %d",nUniverse_index_u);
			if(cGeometry.p_vUniverse[i].p_bIsMoved)
			{
				fprintf(fpNewInpFilePtr,"  move = ");
				for(int j = 0 ; j < 3 ; ++j)
				{
					fprintf(fpNewInpFilePtr,"%.16g ",cGeometry.p_vUniverse[i].p_dOrigin[j]);
				}
			}
			if(cGeometry.p_vUniverse[i].p_bIsRotated)
			{
				fprintf(fpNewInpFilePtr,"  rotate = ");
				for(int j = 0 ; j < 3 ; ++j)
				{
					for(int k = 0 ; k < 3 ; ++k)
					{
						fprintf(fpNewInpFilePtr,"%.16g ",cGeometry.p_vUniverse[i].p_dRotation[j][k]);						
					}
				}
			}
			if(cGeometry.p_vUniverse[i].p_bIsLattice)
			{
				fprintf(fpNewInpFilePtr,"  lat = %d",cGeometry.p_vUniverse[i].p_nLatType);
				fprintf(fpNewInpFilePtr,"  scope = ");
				if(cGeometry.p_vUniverse[i].p_nLatType == 1)
				{
					for(int j = 0 ; j < 3 ; ++j)
					{
						fprintf(fpNewInpFilePtr,"%d ",cGeometry.p_vUniverse[i].p_nScope[j]);
					}
					fprintf(fpNewInpFilePtr,"  pitch = ");
					for(int j = 0 ; j < 3 ; ++j)
					{
						fprintf(fpNewInpFilePtr,"%g ",cGeometry.p_vUniverse[i].p_dPitch[j]);
					}

					vector<int> nScope(3);
					vector<int> nProduct(3);
					for(int j = 0 ; j < 3 ; ++j)
					{
						nScope[j]=cGeometry.p_vUniverse[i].p_nScope[j];
					}
					nProduct[0]=nScope[1]*nScope[2];
					nProduct[1]=nScope[0]*nScope[2];
					nProduct[2]=nScope[0]*nScope[1];
					int nProductXYZ=nScope[0]*nScope[1]*nScope[2];
					if (nProductXYZ == 1)  // 1 1 1
						fprintf(fpNewInpFilePtr,"  fill = %d\n",cGeometry.p_vUniverse[i].p_vFillLatUnivIndexU[0]);
					else if (nScope[0] != 1 && nScope[1] != 1 && nScope[2] != 1) // three dimensions
					{
						///本次修复bug的目标。
						fprintf(fpNewInpFilePtr,"  fill = \n");
						for (int ii = 0;ii < cGeometry.p_vUniverse[i].p_nScope[2];++ ii)
						{
							fprintf(fpNewInpFilePtr,"    ");
							for(int jj = 0;jj < cGeometry.p_vUniverse[i].p_nScope[1];++ jj)
							{
								for(int kk = 0;kk < cGeometry.p_vUniverse[i].p_nScope[0];++ kk)
								{
									fprintf(fpNewInpFilePtr," %d",cGeometry.p_vUniverse[i].p_vFillLatUnivIndexU[ii*nScope[0]*nScope[1]+jj*nScope[1]+ kk]);
								}
								fprintf(fpNewInpFilePtr,"\n");
							}
						}
					}
					else // one or two dimensions
					{
						bool istwo = true;
						for(int j = 0 ; j < 3 ; ++j)
						{
							if(nScope[j] != 1 && nProduct[j] == 1) // one dimensions
							{
								PrintVaryVec(fpNewInpFilePtr,1, cGeometry.p_vUniverse[i].p_nScope[j],cGeometry.p_vUniverse[i].p_vFillLatUnivIndexU,"%d "," fill = ");///一维的情况
								fprintf(fpNewInpFilePtr,"\n");
								istwo = false;
							}
						}
						if(istwo == true)///二维的情况
						{
							for(int j = 0 ; j < 3 ; ++j)
							{
								int nOne,nTwo;
								if(nScope[j]==1&&nProduct[j]!=1)
								{
									if(j == 0) 
									{
										nOne = 1;
										nTwo = 2;
									}
									else if(j == 1)
									{
										nOne = 0;
										nTwo = 2;
									}
									else///j==2
									{
										nOne = 0;
										nTwo = 1;
									}
									fprintf(fpNewInpFilePtr,"  fill = \n");
									for (int ii = 0;ii < cGeometry.p_vUniverse[i].p_nScope[nTwo];++ ii)
									{
										fprintf(fpNewInpFilePtr,"    ");
										for(int jj = 0;jj < cGeometry.p_vUniverse[i].p_nScope[nOne];++ jj)
										{
											fprintf(fpNewInpFilePtr," %d",cGeometry.p_vUniverse[i].p_vFillLatUnivIndexU[ii*cGeometry.p_vUniverse[i].p_nScope[nOne] + jj]);
										}
										fprintf(fpNewInpFilePtr,"\n");
									}									
								}
							}
						}
					}						
				}
				else if(cGeometry.p_vUniverse[i].p_nLatType == 2)
				{
					for(int j = 0 ; j < 2 ; ++j)
					{
						fprintf(fpNewInpFilePtr,"%d ",cGeometry.p_vUniverse[i].p_nScope[j]);
					}
					fprintf(fpNewInpFilePtr,"  pitch = ");
					for(int j = 0 ; j < 2 ; ++j)
					{
						fprintf(fpNewInpFilePtr,"%g ",cGeometry.p_vUniverse[i].p_dPitch[j]);
					}
					fprintf(fpNewInpFilePtr,"  sita = ",cGeometry.p_vUniverse[i].p_dSita);
					fprintf(fpNewInpFilePtr,"  fill = \n");
					for (int ii = 0;ii < cGeometry.p_vUniverse[i].p_nScope[1];++ ii)
					{
						fprintf(fpNewInpFilePtr,"    ");
						for(int jj = 0; jj < cGeometry.p_vUniverse[i].p_nScope[0]; ++jj)
						{
							fprintf(fpNewInpFilePtr," %d",cGeometry.p_vUniverse[i].p_vFillLatUnivIndexU[ii*cGeometry.p_vUniverse[i].p_nScope[0] + jj]);
						}
						fprintf(fpNewInpFilePtr,"\n");
					}
				}
			}
			else// NOT Lattice, print cells
			{
				fprintf(fpNewInpFilePtr,"\n");
				for (int j = 0 ; j < cGeometry.p_vUniverse[i].p_nContainCellNum ; ++j )
				{					
					//1. CellID 
					int cell_index = cGeometry.p_vUniverse[i].p_vFillCellsIndex[j] ;
					int cell_index_u = cGeometry.p_vUniverse[i].p_vFillCellsIndexU[j] ;
					fprintf(fpNewInpFilePtr,"cell  %-3d  ",cell_index_u);
					//2. Surfs
					int TotSurfNum = cGeometry.p_vCell[cell_index].p_vBooleanSurfaces.size();
					for (int k = 0 ; k < TotSurfNum; ++k)
					{
						int surf = cGeometry.p_vCell[cell_index].p_vBooleanSurfaces[k];							
						switch (surf)
						{
						case 1000001:
							fprintf(fpNewInpFilePtr,"(");
							break;
						case 1000002:
							fprintf(fpNewInpFilePtr,")");
							break;
						case 1000003:
							fprintf(fpNewInpFilePtr,"&");
							break;
						case 1000004:
							fprintf(fpNewInpFilePtr,":");
							break;
						case 1000005:
							fprintf(fpNewInpFilePtr,"!");
							break;
						default:
							int nSurf_index = abs(surf);
							int nSurf_u = cGeometry.p_OSurfaceIndex.GetIndexU(nSurf_index);
							nSurf_u = surf > 0 ? nSurf_u : -nSurf_u;
							fprintf(fpNewInpFilePtr," %d ",nSurf_u);
							break;
						}
					}
					//3.mat
					if (!cGeometry.p_vCell[cell_index].p_bIsExpdMat)
					{
						int mat_index_u = cGeometry.p_vCell[cell_index].p_nMatIndexU;
						if (mat_index_u != 0)
							fprintf(fpNewInpFilePtr," mat = %d  ",mat_index_u);
					}
					else // Lattice mat, IMPORTANT: lattice material mat_index_u must be natural incremental numbers
					{
						vector<vector<int> > vTempExpdCells(0);
						vector<int> vTempCells(0);
						cGeometry.ExpdGlobalCell(vTempExpdCells,vTempCells,cell_index_u,0);
						int nExpdMatNum = vTempExpdCells.size();
						int nStartMat = cGeometry.GetCellMat(vTempExpdCells[0]);
						int nStartMatU = cMaterial.p_OMatSetIndex.GetIndexU(nStartMat);						
						if (nExpdMatNum > 1)
						{
							fprintf(fpNewInpFilePtr," mat = %d:%d  ",nStartMatU, nStartMatU + nExpdMatNum - 1);
						}
						else
						{
							fprintf(fpNewInpFilePtr," mat = %d  ",nStartMatU);
						}
						
						// for check
						int nEndMatU = cMaterial.p_OMatSetIndex.GetIndexU(cGeometry.GetCellMat(vTempExpdCells[nExpdMatNum - 1]));
						if (nEndMatU != nStartMatU + nExpdMatNum - 1)
						{
							fprintf(fpNewInpFilePtr,"## ERROR: incorrect lattice material\n");
						}
					}
					//4.  tem=   vol=  void = inner=  fill=
					if(cGeometry.p_vCell[cell_index].p_dTemp != 2.53E-8)fprintf(fpNewInpFilePtr,"  tmp = %g  ",cGeometry.p_vCell[cell_index].p_dTemp/1.3806505E-23*(1.6022E-19)/1.0E-6);
					if(cGeometry.p_vCell[cell_index].p_dVol != 1.0)fprintf(fpNewInpFilePtr,"  vol = %g  ",cGeometry.p_vCell[cell_index].p_dVol);
					if(cGeometry.p_vCell[cell_index].p_nImp != 1)fprintf(fpNewInpFilePtr,"  void = %d  ",1-cGeometry.p_vCell[cell_index].p_nImp);			
					if(cGeometry.p_vCell[cell_index].p_nFillUnivIndexU > 0)fprintf(fpNewInpFilePtr,"  fill = %d  ",cGeometry.p_vCell[cell_index].p_nFillUnivIndexU);
					if(cGeometry.p_vCell[cell_index].p_bIsInnerCell)fprintf(fpNewInpFilePtr,"  inner = %d  ",1);
					fprintf(fpNewInpFilePtr,"\n");
				}
			}
			fprintf(fpNewInpFilePtr,"\n");//Blank line
		}
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////   2. Print SURFACE   ///////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	{
		fprintf(fpNewInpFilePtr,"SURFACE\n");
		int nTotSurfNum = cGeometry.p_vSurface.size();
		for(int i = 1 ; i < nTotSurfNum ; ++i )
		{
			fprintf(fpNewInpFilePtr,"surf  %-3d  ",cGeometry.p_OSurfaceIndex.p_vIndexU[i]);
			fprintf(fpNewInpFilePtr,"%-3s  ",CvtSurfType(cGeometry.p_vSurface[i].p_nType).c_str());
			for(int j = 0;j < cGeometry.p_vSurface[i].p_vParas.size();++j)
			{
				fprintf(fpNewInpFilePtr," %.12g ",cGeometry.p_vSurface[i].p_vParas[j]);//Surf Paras
			}
			if(cGeometry.p_vSurface[i].p_nBoundCond == 1)
				fprintf(fpNewInpFilePtr," bc = 1");//Bound condition

			fprintf(fpNewInpFilePtr,"\n");
		}
		
		fprintf(fpNewInpFilePtr,"\n");//Blank line
	}
	
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////  3. Print MATERIAL  ////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	{
		fprintf(fpNewInpFilePtr,"MATERIAL\n");
		int nTotMatNum = cMaterial.p_vMatSet.size();
		for(int i = 1 ; i < nTotMatNum ; ++i )
		{
			fprintf(fpNewInpFilePtr,"mat %-3d  %.10g\n", cMaterial.p_OMatSetIndex.GetIndexU(i), cMaterial.GetMatUserDen(i));
			for(int j = 0;j < cMaterial.GetMatTotNucNum(i);++j)
			{
				fprintf(fpNewInpFilePtr,"         %-10s",cMaterial.GetMatNucID(i,j).p_chIdStr);
				fprintf(fpNewInpFilePtr,"  %E\n",cMaterial.GetMatNucUserDen(i,j));
			}
			for (int k = 0;k < cMaterial.p_vMatSet[i].p_dMatTotSabNucNum;++ k)
			{
				fprintf(fpNewInpFilePtr,"sab %-3d  %s\n",cMaterial.p_OMatSetIndex.p_vIndexU[i],cMaterial.GetMatSabNucID(i,k).p_chIdStr);		
			}
		}
		if(cAceData.p_bUseErgBinMap != 1 || cAceData.p_bUseProbTable != 1)//ceACE模块
		{
			fprintf(fpNewInpFilePtr,"CeAce  ErgBinHash = %d  pTable = %d\n",cAceData.p_bUseErgBinMap, cAceData.p_bUseProbTable);
		}
		if(cAceData.p_bIsMultiGroup)//MgACell模块
		{
			fprintf(fpNewInpFilePtr,"MgAce  ErgGrp = %d\n",cAceData.p_nMltGrpNum);
		}
		
		fprintf(fpNewInpFilePtr,"\n");
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////  4. Print CRITICALITY  ////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	if (cCriticality.p_nNeuNumPerCyc > 0)
	{
		///////////////////////////////////// OutPut Kcode //////////////////////////////////////////////////
		fprintf(fpNewInpFilePtr,"CRITICALITY\n");
		fprintf(fpNewInpFilePtr,"PowerIter  keff0 = %f   population = %d %d %d\n",cCriticality.p_dKeffFinal,cCriticality.p_nNeuNumPerCyc,cCriticality.p_nInactCycNum,cCriticality.p_nTotCycNum);
		fprintf(fpNewInpFilePtr,"InitSrc  %s = ",CvtInitSrcType(cCriticality.p_nKsrcType).c_str());
		for(int j = 0;j < cCriticality.p_vKsrcPara.size();++j)
		{
			fprintf(fpNewInpFilePtr,"%g ",cCriticality.p_vKsrcPara[j]);				
		}
		fprintf(fpNewInpFilePtr,"\n");
		
		if(ORNG.RN_MULT != 3512401965023503517 || ORNG.RN_SEED0 != 1 || ORNG.RN_STRIDE != 100000)//RNG模块
		{
			fprintf(fpNewInpFilePtr,"RNG ");
			if(ORNG.RN_MULT == 19073486328125) fprintf(fpNewInpFilePtr,"  Type = 1");
			else if(ORNG.RN_MULT == 3512401965023503517) fprintf(fpNewInpFilePtr,"  Type = 2");
			else fprintf(fpNewInpFilePtr,"  Type = 3");//=9219741426499971445

			if(ORNG.RN_SEED0 != 1)
			{
				fprintf(fpNewInpFilePtr,"  Seed = %u",ORNG.RN_SEED0);
			}
			if(ORNG.RN_STRIDE != 100000)
			{
				fprintf(fpNewInpFilePtr,"  Stride = %u",ORNG.RN_STRIDE);
			}

			fprintf(fpNewInpFilePtr,"\n");
		}
#ifdef _IS_PARALLEL_
		if(OParallel.p_bUseFastBank != 1)//parallelBank模块
		{
			fprintf(fpNewInpFilePtr,"ParallelBank  %d\n",OParallel.p_bUseFastBank);
		}
#endif

		fprintf(fpNewInpFilePtr,"\n");			
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////  5. Print BURNUP  ////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	if (cBurnup.p_bIsWithBurnup)
	{
		if (cBurnup.p_nBurnStep == cBurnup.p_nTotBurnStep)
		{
			fprintf(fpNewInpFilePtr,"## All Burn Steps Finished!\n\n");
		}
		else
		{
			fprintf(fpNewInpFilePtr,"BURNUP\n");
			fprintf(fpNewInpFilePtr,"BurnCell    ");
			for(int i = 0; i < cBurnup.p_vBurnableCell.size(); ++i)
			{
				fprintf(fpNewInpFilePtr,"%d ",cBurnup.p_vBurnableCell[i]);
			}
			fprintf(fpNewInpFilePtr,"\n");
			PrintVaryVec(fpNewInpFilePtr,2 + cBurnup.p_nBurnStep, cBurnup.p_vStepBurnTime.size(),cBurnup.p_vStepBurnTime,"%g ","TimeStep    "); // only print rest steps
			fprintf(fpNewInpFilePtr,"\n");
			PrintVaryVec(fpNewInpFilePtr,2 + cBurnup.p_nBurnStep, cBurnup.p_vStepBurnPowerDen.size(),cBurnup.p_vStepBurnPowerDen,"%g ","Power       ");// only print rest steps
			fprintf(fpNewInpFilePtr,"\n");
			if(cBurnup.p_nInnerStepNum != 10)
				fprintf(fpNewInpFilePtr,"SubStep     %d\n",cBurnup.p_nInnerStepNum);
			if (cBurnup.p_dAbsorpFracCutoff != 0.99)
				fprintf(fpNewInpFilePtr,"Inherent    %g\n",cBurnup.p_dAbsorpFracCutoff);
			fprintf(fpNewInpFilePtr,"AceLib      %s\n",cBurnup.p_chAceLibForBurn);
			fprintf(fpNewInpFilePtr,"Depthlib    %s\n",cBurnup.p_chDepthLibForBurn);
			if(cBurnup.p_nBurnStepStrategy != 0)
				fprintf(fpNewInpFilePtr,"Strategy    1\n");//,cBurnup.p_nBurnStepStrategy); // p_nBurnStepStrategy = -1 for corrector step
			if(cBurnup.p_nBurnSolver != 2)
				fprintf(fpNewInpFilePtr,"Solver      %d\n",cBurnup.p_nBurnSolver);
#ifdef _IS_PARALLEL_
			if(OParallel.p_bIsParallelBurn != 0)
				fprintf(fpNewInpFilePtr,"Parallel    %d\n",OParallel.p_bIsParallelBurn);
#endif
			if (cBurnup.p_nXeEqFlag != 0)
				fprintf(fpNewInpFilePtr,"XeEq        %n\n",cBurnup.p_dXeEqFactor);
			if (cBurnup.p_dNAECFInput != 0)
				fprintf(fpNewInpFilePtr,"NAECF       %g\n",cBurnup.p_dNAECFInput);
			int nCellSize = cBurnup.p_vCellsForOutput.size();
			if (nCellSize > 0)
			{
				fprintf(fpNewInpFilePtr,"Outputcell  ");
				for (int i = 0;i < nCellSize;i++)
				{
					if (i > 0)fprintf(fpNewInpFilePtr,"            ");
					fprintf(fpNewInpFilePtr,"%-30s\n",Output.PrintCellVec(cBurnup.p_vCellsForOutput[i]));				
				}
			}

			fprintf(fpNewInpFilePtr,"\n");
		}
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////  6. Print TALLY  ///////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	if(cTally.p_bIsWithCellTally || cTally.p_bIsWithMeshTally || (cTally.p_bIsWithCsTally && !cBurnup.p_bIsWithBurnup) || (cBurnup.p_bIsWithBurnup && cBurnup.p_nBurnTallyStartPos > 0))
	{
		fprintf(fpNewInpFilePtr,"TALLY\n");
		if(cTally.p_bIsWithCellTally)
		{
			for(int i = 0; i < cTally.p_vCellTally.size();i++)
			{
				fprintf(fpNewInpFilePtr,"CellTally %d  ",cTally.p_vCellTally[i].p_nTallyID);
				fprintf(fpNewInpFilePtr,"Type = %d  ",cTally.p_vCellTally[i].p_nTallyType);
				if(cTally.p_vCellTally[i].p_vErgBins.size() != 0)
				{
					fprintf(fpNewInpFilePtr,"Energy = ");
					for(int j = 0 ; j < cTally.p_vCellTally[i].p_vErgBins.size(); ++j)
					{
						fprintf(fpNewInpFilePtr,"%e ",cTally.p_vCellTally[i].p_vErgBins[j]);
					}
				}
				if(cTally.p_vCellTally[i].p_vFilter.size() != 0)	
				{
					fprintf(fpNewInpFilePtr,"Filter = ");
					for(int j = 0 ; j < cTally.p_vCellTally[i].p_vFilter.size(); ++j)
					{						
						fprintf(fpNewInpFilePtr,"%d ",cTally.p_vCellTally[i].p_vFilter[j]);
					}			
				}
				if(cTally.p_vCellTally[i].p_vIntgrlBins.size() != 0)	
				{
					vector<int> p_vIntgrlBins;
					p_vIntgrlBins.resize(cTally.p_vCellTally[i].p_vIntgrlBins.size());
					p_vIntgrlBins[0] = cTally.p_vCellTally[i].p_vIntgrlBins[0];
					for(int j = p_vIntgrlBins.size()-1;j > 0; j--)
					{
						p_vIntgrlBins[j] = cTally.p_vCellTally[i].p_vIntgrlBins[j] - cTally.p_vCellTally[i].p_vIntgrlBins[j-1];
					}

					PrintVaryVec(fpNewInpFilePtr,1, p_vIntgrlBins.size(), p_vIntgrlBins,"%d "," Integral = ");
				}
				fprintf(fpNewInpFilePtr,"\n             Cell = ");
				int nCellSize = cTally.p_vCellTally[i].p_vCellVecListU.size();
				for (int j = 0;j < nCellSize;j++)
				{
					if (j > 0)fprintf(fpNewInpFilePtr,"                    ");
					fprintf(fpNewInpFilePtr,"%-30s\n",Output.PrintCellVec(cTally.p_vCellTally[i].p_vCellVecListU[j]));
				}
			}
		}

		if(cTally.p_bIsWithMeshTally)
		{
			for(int i = 0;i < cTally.p_vMeshTally.size();i++)
			{
				fprintf(fpNewInpFilePtr,"MeshTally %d ",cTally.p_vMeshTally[i].p_nTallyID);
				fprintf(fpNewInpFilePtr,"  Type = %d ",cTally.p_vMeshTally[i].p_nTallyType);
				if(cTally.p_vMeshTally[i].p_vErgBins.size() != 0)
				{
					fprintf(fpNewInpFilePtr,"  Energy = ");
					for(int j = 0 ; j < cTally.p_vMeshTally[i].p_vErgBins.size(); ++j)
					{
						fprintf(fpNewInpFilePtr,"%e ",cTally.p_vMeshTally[i].p_vErgBins[j]);
					}
				}
				fprintf(fpNewInpFilePtr,"  Scope = ");
				for(int j = 0 ; j < 3 ; ++j)
				{
					fprintf(fpNewInpFilePtr,"%d ",cTally.p_vMeshTally[i].p_OTallyMesh.p_nMeshScope[j]);
				}
				fprintf(fpNewInpFilePtr,"  Bound = ");
				for(int j = 0 ; j < 3 ; ++j)
				{
					fprintf(fpNewInpFilePtr,"%g %g ",cTally.p_vMeshTally[i].p_OTallyMesh.p_dBoundMin[j],cTally.p_vMeshTally[i].p_OTallyMesh.p_dBoundMax[j]);
				}
				fprintf(fpNewInpFilePtr,"\n");
			}

			fprintf(fpNewInpFilePtr,"\n");
		}

		if(cTally.p_bIsWithCsTally)
		{
			for(int i = 0;i < cTally.p_vOneGroupCsTally.size();i++)
			{
				if (cTally.p_vOneGroupCsTally[i].p_bIsForBurnup)
				{
					continue;
				}
				fprintf(fpNewInpFilePtr,"CsTally %d ",cTally.p_vOneGroupCsTally[i].p_nTallyID);
				fprintf(fpNewInpFilePtr,"   Cell = ");
				fprintf(fpNewInpFilePtr,"%-30s ",Output.PrintCellVec(cTally.p_vOneGroupCsTally[i].p_vCellVecU));
				int nMat = cMaterial.p_OMatSetIndex.GetIndexU(cTally.p_vOneGroupCsTally[i].p_nMat);
				fprintf(fpNewInpFilePtr,"  Mat = %d ",nMat);
				fprintf(fpNewInpFilePtr,"  MT = ");
				int nNucSize=cTally.p_vOneGroupCsTally[i].p_vNucMTs.size();
				for (int j =0;j < nNucSize;j++)
				{
					int nMTength=cTally.p_vOneGroupCsTally[i].p_vNucMTs[j].size();
					for(int k = 0;k < nMTength;k++)
					{
						fprintf(fpNewInpFilePtr,"%d ",cTally.p_vOneGroupCsTally[i].p_vNucMTs[j][k]);
					}
					if(j < nNucSize-1)fprintf(fpNewInpFilePtr," , ");
				}
				fprintf(fpNewInpFilePtr,"\n");
			}		
		}

		if(!cTally.p_bUseTallyCellMap || cTally.p_bUseTallyCellBinMap || cTally.p_bUseUnionCsTally)
		{
			fprintf(fpNewInpFilePtr,"AcceTally  ");
			if(!cTally.p_bUseTallyCellMap)
			{
				fprintf(fpNewInpFilePtr,"  Map = 0 ");
			}
			else
			{
				if(cTally.p_bUseTallyCellBinMap)
				{
					fprintf(fpNewInpFilePtr,"  Map = 2 ");
				}
			}
			if(cTally.p_bUseUnionCsTally) fprintf(fpNewInpFilePtr,"  Union = %d",cTally.p_bUseUnionCsTally);
			fprintf(fpNewInpFilePtr,"\n");
		}
		fprintf(fpNewInpFilePtr,"\n");
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////  7. Print PRINT  ///////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	if (!p_bIsMatPrint || !p_bIsKeffPrint || p_bIsSrcPrint || !p_bIsCellTallyPrint || !p_bIsMeshTallyPrint || !p_bIsCsTallyPrint || p_bIsTallyPrintPerCyc || p_bIsInputFilePrint)
	{
		fprintf(fpNewInpFilePtr,"PRINT\n");
		if(!p_bIsMatPrint)          fprintf(fpNewInpFilePtr,"Mat        %d\n", Output.p_bIsMatPrint);
		if(!p_bIsKeffPrint)         fprintf(fpNewInpFilePtr,"Keff       %d\n", Output.p_bIsKeffPrint);
		if(p_bIsSrcPrint)           fprintf(fpNewInpFilePtr,"Source     %d\n", Output.p_bIsSrcPrint);
		if(!p_bIsCellTallyPrint)    fprintf(fpNewInpFilePtr,"CellTally  %d\n", Output.p_bIsCellTallyPrint);
		if(!p_bIsMeshTallyPrint)    fprintf(fpNewInpFilePtr,"MeshTally  %d\n", Output.p_bIsMeshTallyPrint);
		if(!p_bIsCsTallyPrint)      fprintf(fpNewInpFilePtr,"CsTally    %d\n", Output.p_bIsCsTallyPrint);
		if(p_bIsTallyPrintPerCyc)   fprintf(fpNewInpFilePtr,"CycTally   %d\n", Output.p_bIsTallyPrintPerCyc);
		if(p_bIsInputFilePrint)     fprintf(fpNewInpFilePtr,"InpFile    %d\n", Output.p_bIsInputFilePrint);
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////  8. Print PLOT  ////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// different format of PLOT for new version
	
	/*if(cPlot.p_bIsPlot)
	{
		fprintf(fpNewInpFilePtr,"PLOT");
		if(cPlot.p_unColorScheme != 1 || !cCalMode.p_bSkipCalculation)//不等于默认值-	Continue-calculation=0为默认值等价于cCalMode.p_bSkipCalculation==true
		{
			if(cPlot.p_unColorScheme != 1) fprintf(fpNewInpFilePtr,"   ColorScheme = %u ",cPlot.p_unColorScheme);
			if(!cCalMode.p_bSkipCalculation) fprintf(fpNewInpFilePtr,"  Continue-calculation = %d",!cCalMode.p_bSkipCalculation);
		}
		fprintf(fpNewInpFilePtr,"\n");
		for(int i = 0;i < cPlot.p_vPlotInput.size();i++)
		{
			fprintf(fpNewInpFilePtr,"PlotID  %d ",cPlot.p_vPlotInput[i].p_nPlotID);
			fprintf(fpNewInpFilePtr," Type = ");
			if (cPlot.p_vPlotInput[i].p_nPlotType == 1)
			{
				fprintf(fpNewInpFilePtr,"Slice  ");
			}
			else if (cPlot.p_vPlotInput[i].p_nPlotType == 2)
			{
				fprintf(fpNewInpFilePtr,"Box  ");
			}

			fprintf(fpNewInpFilePtr," Color = ");
			switch(cPlot.p_vPlotInput[i].p_nColorType)
			{
			case 1:
				fprintf(fpNewInpFilePtr,"MAT  ");
			case 2:
				fprintf(fpNewInpFilePtr,"CELL  ");
			case 3:
				fprintf(fpNewInpFilePtr,"SURF  ");
			case 4:
				fprintf(fpNewInpFilePtr,"MATSURF  ");
			case 5:
				fprintf(fpNewInpFilePtr,"CELLSURF  ");
			default:
				fprintf(fpNewInpFilePtr," ");
			}

			if (cPlot.p_vPlotInput[i].p_nPlotType == 1) // Slice
			{
				fprintf(fpNewInpFilePtr," Pixels = %d %d",cPlot.p_vPlotInput[i].p_nPixel_1,cPlot.p_vPlotInput[i].p_nPixel_2);
				fprintf(fpNewInpFilePtr," Vertexes = %g %g %g %g %g %g %g %g %g",
					cPlot.p_vPlotInput[i].p_OVertex1._x,cPlot.p_vPlotInput[i].p_OVertex1._y,cPlot.p_vPlotInput[i].p_OVertex1._z,
					cPlot.p_vPlotInput[i].p_OVertex2._x,cPlot.p_vPlotInput[i].p_OVertex2._y,cPlot.p_vPlotInput[i].p_OVertex2._z,
					cPlot.p_vPlotInput[i].p_OVertex3._x,cPlot.p_vPlotInput[i].p_OVertex3._y,cPlot.p_vPlotInput[i].p_OVertex3._z);
			}
			else if (cPlot.p_vPlotInput[i].p_nPlotType == 2) // box
			{
				fprintf(fpNewInpFilePtr," Pixels = %d %d %d",cPlot.p_vPlotInput[i].p_nPixel_1,cPlot.p_vPlotInput[i].p_nPixel_2, cPlot.p_vPlotInput[i].p_nPixel_3);
				fprintf(fpNewInpFilePtr," Vertexes = %g %g %g %g %g %g %g %g %g %g %g %g",
					cPlot.p_vPlotInput[i].p_OVertex1._x,cPlot.p_vPlotInput[i].p_OVertex1._y,cPlot.p_vPlotInput[i].p_OVertex1._z,
					cPlot.p_vPlotInput[i].p_OVertex2._x,cPlot.p_vPlotInput[i].p_OVertex2._y,cPlot.p_vPlotInput[i].p_OVertex2._z,
					cPlot.p_vPlotInput[i].p_OVertex3._x,cPlot.p_vPlotInput[i].p_OVertex3._y,cPlot.p_vPlotInput[i].p_OVertex3._z,
					cPlot.p_vPlotInput[i].p_OVertex4._x,cPlot.p_vPlotInput[i].p_OVertex4._y,cPlot.p_vPlotInput[i].p_OVertex4._z);
			}
			fprintf(fpNewInpFilePtr,"\n");
		}
		fprintf(fpNewInpFilePtr,"\n");
	}*/

	///////////////////////////////////// Complete //////////////////////////////////////////////////
	fclose(fpNewInpFilePtr);

	return;
}


string CvtSurfType(int intsurf)
{
	if(intsurf == 1 ){return "P";}
	if(intsurf == 2) {return "PX";}
	if(intsurf == 3) {return "PY";}
	if(intsurf == 4 ){return "PZ";}
	if(intsurf == 5) {return "SO";}
	if(intsurf == 6) {return "S";}
	if(intsurf == 7 ){return "SX";}
	if(intsurf == 8) {return "SY";}
	if(intsurf == 9 ){return "SZ";}
	if(intsurf == 10){return "C/X";}
	if(intsurf == 11){return "C/Y";}
	if(intsurf == 12){return "C/Z";}
	if(intsurf == 13){return "CX";}
	if(intsurf == 14){return "CY";}
	if(intsurf == 15){return "CZ";}
	if(intsurf == 16){return "K/X";}
	if(intsurf == 17){return "K/Y";}
	if(intsurf == 18){return "K/Z";}
	if(intsurf == 19){return "KX";}
	if(intsurf == 20){return "KY";}
	if(intsurf == 21){return "KZ";}
	return "Error";
}

string CvtInitSrcType(int intsurf)
{
	if(intsurf == 1 ){ return "POINT";}
	if(intsurf == 2){ return "SLAB";}
	if(intsurf == 3){ return "SPH";}
	if(intsurf == 4 ){ return "CYL/X";}
	if(intsurf == 5){ return "CYL/Y";}
	if(intsurf == 6){ return "CYL/Z";}
	return "Error";
}


template<typename type>	static void PrintVaryVec(FILE *fptr,int nHead, int nSize, vector<type> &vT,char *pFmt,string cCardStr)
{
	//// print parameters of varying length
	///nHead代表第一个需要输出的元素的下标加1，nEnd表示总的大小。如果nEnd=1，那么它只有一个元素0
	fprintf(fptr,"%s",cCardStr.c_str());
	int nLength = nSize - nHead;
	if(nLength == 0)
	{
		fprintf(fptr,pFmt,vT[nHead-1]);
	}
	else
	{
		int nlastCounter = 1;
		int nCurrentCounter = 1;
		for(int ii = nHead; ii < nSize; ++ii)
		{
			if(vT[ii] - vT[ii-1] == 0)
			{
				nCurrentCounter = nCurrentCounter + 1;
			}
			if(nCurrentCounter == 1)
			{
				fprintf(fptr,pFmt,vT[ii-1]);
				if(ii == nSize-1)  fprintf(fptr,pFmt,vT[ii]);
			}
			else if(nCurrentCounter == nlastCounter)
			{
				fprintf(fptr,pFmt,vT[ii-1]);
				fprintf(fptr,"*%d ",nCurrentCounter);
				if(ii == nSize-1) fprintf(fptr,pFmt,vT[ii]);
				nCurrentCounter = 1;
			}
			else if(ii == nSize-1)
			{
				fprintf(fptr,pFmt,vT[ii]);
				fprintf(fptr,"*%d ",nCurrentCounter);
			}
			nlastCounter = nCurrentCounter;
		}
	}
}