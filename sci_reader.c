///////////////////////////////////////////////////////////////////////////////
///
/// \file   sci_reader.c
///
/// \brief  Read mesh and simulation data defined by SCI
///
/// \author Mingang Jin, Qingyan Chen
///         Purdue University
///         Jin55@purdue.edu, YanChen@purdue.edu
///         Wangda Zuo
///         University of Miami
///         W.Zuo@miami.edu
///
/// \date   8/3/2013
///
///////////////////////////////////////////////////////////////////////////////

#include "sci_reader.h"

///////////////////////////////////////////////////////////////////////////////
/// Read the basic index information from input.cfd
///
/// Specific method for advection will be selected according to the variable 
/// type.
///
///\param para Pointer to FFD parameters
///\param var Pointer to FFD simulation variables
///
///\return 0 if no error occurred
///////////////////////////////////////////////////////////////////////////////
int read_sci_max(PARA_DATA *para, REAL **var) {  
  char string[400];

  // Open the file
  if((file_params=fopen(para->inpu->parameter_file_name,"r")) == NULL) {
    fprintf(stderr,"Error:can not open the file \"%s\".\n",
      para->inpu->parameter_file_name);
    return 1;
  }

  // Get the first line for the length in X, Y and Z directions
  fgets(string, 400, file_params);
  sscanf(string,"%f %f %f", &para->geom->Lx, &para->geom->Ly, &para->geom->Lz);

  // Get the second line for the number of cells in X, Y and Z directions
  fgets(string, 400, file_params);
  sscanf(string,"%d %d %d", &para->geom->imax, &para->geom->jmax,
    &para->geom->kmax);

  fclose(file_params); 
  return 0;
} // End of read_sci_max()


///////////////////////////////////////////////////////////////////////////////
/// Read other information from input.cfd
///
///\param para Pointer to FFD parameters
///\param var Pointer to FFD simulation variables
///\param var_type Type of variable
///\param BINDEX Pointer to boundary index
///
///\return 0 if no error occurred
///////////////////////////////////////////////////////////////////////////////
int read_sci_input(PARA_DATA *para, REAL **var, int **BINDEX) {
  int i, j, k;
  int ii,ij,ik;
  REAL tempx, tempy, tempz;
  REAL Lx = para->geom->Lx;
  REAL Ly = para->geom->Ly;
  REAL Lz = para->geom->Lz;
  REAL *gx = var[GX], *gy = var[GY], *gz = var[GZ];
  REAL *x = var[X], *y = var[Y], *z = var[Z];
  int IWWALL,IEWALL,ISWALL,INWALL,IBWALL,ITWALL;
  int SI,SJ,SK,EI,EJ,EK,FLTMP;
  REAL TMP,MASS,U,V,W;
  REAL trefmax;
  char name[100];
  int imax = para->geom->imax;
  int jmax = para->geom->jmax;
  int kmax = para->geom->kmax;
  int index=0;
  int IMAX = imax+2, IJMAX = (imax+2)*(jmax+2); 
  char *temp, string[400];
  REAL *delx,*dely,*delz;
  REAL *flagp = var[FLAGP],*flagu = var[FLAGU],*flagv = var[FLAGV],*flagw = var[FLAGW];
  int bcnameid = -1;

  // Open the parameter file
  if((file_params=fopen(para->inpu->parameter_file_name,"r")) == NULL ) { 
    sprintf(msg,"read_sci_input(): Could not open the file \"%s\".", 
            para->inpu->parameter_file_name);
    ffd_log(msg, FFD_ERROR);
    return 1;
  }

  sprintf(msg, "read_sci_input(): Start to read sci input file %s",
          para->inpu->parameter_file_name);
  ffd_log(msg, FFD_NORMAL);

  // Ingore the first and second lines
  temp = fgets(string, 400, file_params);
  temp = fgets(string, 400, file_params);

  /*---------------------------------------------------------------------------
  | Convert the cell dimensions defined by SCI to coordinates in FFD
  ----------------------------------------------------------------------------*/
  // Allocate temporary memory for diemension of each cell  
  delx = (REAL *) malloc ((imax+2)*sizeof(REAL));
  dely = (REAL *) malloc ((jmax+2)*sizeof(REAL));
  delz = (REAL *) malloc ((kmax+2)*sizeof(REAL));

  if( !delx || !dely ||!delz ) {
    fprintf ( stderr, "Cannot allocate memory for delx, dely or delz in read_sci_input()\n");
    return 1;
  }

  delx[0]=0;
  dely[0]=0;
  delz[0]=0;

  // Read cell dimensions in X, Y, Z directions
  for(i=1; i<=imax; i++) fscanf(file_params, "%f", &delx[i]); 
  fscanf(file_params,"\n");
  for(j=1; j<=jmax; j++) fscanf(file_params, "%f", &dely[j]); 
  fscanf(file_params,"\n");
  for(k=1; k<=kmax; k++) fscanf(file_params, "%f", &delz[k]); 
  fscanf(file_params,"\n");

  // Store the locations of grid cell surfaces
  // Fixme: use one "temp", not tempx tempy and tempz
  tempx = 0.0; tempy = 0.0; tempz = 0.0;
  for(i=0; i<=imax+1; i++) {
    tempx += delx[i];
    if(i>=imax) tempx = Lx;
    for(j=0; j<=jmax+1; j++)
      for(k=0; k<=kmax+1; k++) var[GX][IX(i,j,k)]=tempx;
  }

  for(j=0; j<=jmax+1; j++) {
    tempy += dely[j];
    if(j>=jmax) tempy = Ly;
    for(i=0; i<=imax+1; i++)
      for(k=0; k<=kmax+1; k++) var[GY][IX(i,j,k)] = tempy;
  }

  for(k=0; k<=kmax+1; k++) {
    tempz += delz[k];
    if(k>=kmax) tempz = Lz;
    for(i=0; i<=imax+1; i++)
      for(j=0; j<=jmax+1; j++) var[GZ][IX(i,j,k)] = tempz;
  }

  // Convert the coordinates for cell furfaces to the coordinates for the cell center
  FOR_ALL_CELL
    if(i<1) 
      x[IX(i,j,k)] = 0;
    else if(i>imax) 
      x[IX(i,j,k)] = Lx;
    else 
      x[IX(i,j,k)] = (REAL) 0.5 * (gx[IX(i,j,k)]+gx[IX(i-1,j,k)]);

    if(j<1)  
      y[IX(i,j,k)] = 0;
    else if(j>jmax) 
      y[IX(i,j,k)] = Ly;
    else 
      y[IX(i,j,k)] = (REAL) 0.5 * (gy[IX(i,j,k)]+gy[IX(i,j-1,k)]);

    if(k<1)  
      z[IX(i,j,k)] = 0;
    else if(k>kmax) 
      z[IX(i,j,k)] = Lz;
    else 
      z[IX(i,j,k)] = (REAL) 0.5 * (gz[IX(i,j,k)]+gz[IX(i,j,k-1)]);
  END_FOR

  // Get the wall property
  fgets(string, 400, file_params);
  sscanf(string,"%d%d%d%d%d%d", &IWWALL, &IEWALL, &ISWALL, 
         &INWALL, &IBWALL, &ITWALL); 

  /*---------------------------------------------------------------------------
  | Read total number of boundary conditions
  ----------------------------------------------------------------------------*/
  fgets(string, 400, file_params);
  sscanf(string,"%d", &para->bc->nb_bc); 
  sprintf(msg, "read_sci_input(): para->bc->nb_bc=%d", para->bc->nb_bc);
  ffd_log(msg, FFD_NORMAL);

  // Allocate the memory for bc name and id
  para->bc->bcname = (char**)malloc(sizeof(char*));
  para->bc->id = (int *)malloc(sizeof(int)*para->bc->nb_bc);
  for(i=0; i<para->bc->nb_bc; i++)
    para->bc->id[i] = -1;
  /*---------------------------------------------------------------------------
  | Read the inlet boundary conditions
  ----------------------------------------------------------------------------*/
  // Get number of inlet boundaries
  fgets(string, 400, file_params);
  sscanf(string,"%d", &para->bc->nb_inlet); 
  sprintf(msg, "read_sci_input(): para->bc->nb_inlet=%d", para->bc->nb_inlet);
  ffd_log(msg, FFD_NORMAL);

  index=0;
  // Setting inlet boundary
  if(para->bc->nb_inlet != 0) {
    // Loop for each inlet boundary
    for(i=1; i<=para->bc->nb_inlet; i++) {
      fgets(string, 400, file_params);
      sscanf(string,"%s%d%d%d%d%d%d%f%f%f%f%f", name, &SI, &SJ, &SK, &EI, 
             &EJ, &EK, &TMP, &MASS, &U, &V, &W);
      bcnameid ++; // starts from -1, thus the first id will be 0
      para->bc->bcname[bcnameid] = (char*)malloc(sizeof(name));
      strcpy(para->bc->bcname[bcnameid], name);
      sprintf(msg, "read_sci_input(): para->bc->bcname[%d]=%s", 
              bcnameid, para->bc->bcname[bcnameid]);
      ffd_log(msg, FFD_NORMAL);
      sprintf(msg, "read_sci_input(): VX=%f, VY=%f, VX=%f, T=%f, Xi=%f", 
              U, V, W, TMP, MASS);
      ffd_log(msg, FFD_NORMAL);
      if(EI==0) { 
        if(SI==1) SI = 0;
        EI = SI + EI; 
        EJ = SJ + EJ - 1; 
        EK = SK + EK - 1;
      }

      if(EJ==0) { 
        if(SJ==1) SJ = 0;
        EI = SI + EI - 1;
        EJ = SJ + EJ;
        EK = SK + EK - 1;
      }

      if(EK==0) { 
        if(SK==1) SK = 0;
        EI = SI + EI - 1;
        EJ = SJ + EJ - 1;
        EK = SK + EK;
      }

      // Assign the inlet boundary condition for each cell
      for(ii=SI; ii<=EI; ii++)
        for(ij=SJ; ij<=EJ; ij++)
          for(ik=SK; ik<=EK; ik++) {
            BINDEX[0][index] = ii;
            BINDEX[1][index] = ij;
            BINDEX[2][index] = ik;
            BINDEX[4][index] = bcnameid;
            index++;
            
            var[TEMPBC][IX(ii,ij,ik)] = TMP;
            var[VXBC][IX(ii,ij,ik)] = U; 
            var[VYBC][IX(ii,ij,ik)] = V; 
            var[VZBC][IX(ii,ij,ik)] = W;
            flagp[IX(ii,ij,ik)] = 0; // Cell flag to be inlet
          } // End of assigning the inlet B.C. for each cell 

    } // End of loop for each inlet boundary
  } // End of setting inlet boundary
    
  if(para->outp->version == DEBUG) 
    printf("Last index of inlet B.C. cell: %d\n", index); 

  /*---------------------------------------------------------------------------
  | Read the outlet boundary conditions
  ----------------------------------------------------------------------------*/
  fgets(string, 400, file_params);
  sscanf(string,"%d",&para->bc->nb_outlet); 
  sprintf(msg, "read_sci_input(): para->bc->nb_outlet=%d", para->bc->nb_outlet);
  ffd_log(msg, FFD_NORMAL);

  if(para->bc->nb_outlet!=0) {
    for(i=1;i<=para->bc->nb_outlet;i++) {
      fgets(string, 400, file_params);
      sscanf(string,"%s%d%d%d%d%d%d%f%f%f%f%f", &name, &SI, &SJ, &SK, &EI, 
             &EJ, &EK, &TMP, &MASS, &U, &V, &W);
      bcnameid++;
      para->bc->bcname[bcnameid] = (char*)malloc(sizeof(name));
      strcpy(para->bc->bcname[bcnameid], name);
      sprintf(msg, "read_sci_input(): para->bc->bcname[%d]=%s", 
              bcnameid, para->bc->bcname[bcnameid]);
      ffd_log(msg, FFD_NORMAL);      
      sprintf(msg, "read_sci_input(): VX=%f, VY=%f, VX=%f, T=%f, Xi=%f", 
              U, V, W, TMP, MASS);
      ffd_log(msg, FFD_NORMAL);

      if(EI==0) { 
        if(SI==1) SI=0;
        EI = SI + EI;
        EJ = SJ + EJ - 1;
        EK = SK + EK - 1;
      }

      if(EJ==0) { 
        if(SJ==1) SJ=0;
        EI = SI+EI-1;
        EJ = SJ+EJ;
        EK = SK+EK-1;
      }

      if(EK==0) { 
        if(SK==1) SK = 0;
        EI = SI+EI-1;
        EJ = SJ+EJ-1;
        EK = SK+EK;
      }
      // Assign the outlet boundary condition for each cell
      for(ii=SI; ii<=EI ;ii++)
        for(ij=SJ; ij<=EJ ;ij++)
          for(ik=SK; ik<=EK; ik++) {
            BINDEX[0][index] = ii;
            BINDEX[1][index] = ij;
            BINDEX[2][index] = ik;
            BINDEX[4][index] = bcnameid;
            index++;

            // Fixme: Why assign TMP, U, V, W for oulet B.C?
            var[TEMPBC][IX(ii,ij,ik)] = TMP;
            var[VXBC][IX(ii,ij,ik)] = U; 
            var[VYBC][IX(ii,ij,ik)] = V; 
            var[VZBC][IX(ii,ij,ik)] = W;
            flagp[IX(ii,ij,ik)] = 2;
          } // End of assigning the outlet B.C. for each cell 
    } // End of loop for each outlet boundary
  } // End of setting outlet boundary

  /*---------------------------------------------------------------------------
  | Read the internal solid block boundary conditions
  ----------------------------------------------------------------------------*/
  fgets(string, 400, file_params);
  sscanf(string, "%d", &para->bc->nb_block); 
  sprintf(msg, "read_sci_input(): para->bc->nb_block=%d", para->bc->nb_block);
  ffd_log(msg, FFD_NORMAL);

  if(para->bc->nb_block!=0) {
    for(i=1; i<=para->bc->nb_block; i++) {
      fgets(string, 400, file_params);
      // X_index_start, Y_index_Start, Z_index_Start, 
      // X_index_End, Y_index_End, Z_index_End, 
      // Thermal Codition (0: Flux; 1:Temperature), Value of thermal conditon
      sscanf(string,"%s%d%d%d%d%d%d%d%f", &name, &SI, &SJ, &SK, &EI, &EJ, &EK, 
                                        &FLTMP, &TMP);
      bcnameid++;
      para->bc->bcname[bcnameid] = (char*)malloc(sizeof(name));
      strcpy(para->bc->bcname[bcnameid], name);
      sprintf(msg, "read_sci_input(): para->bc->bcname[%d]=%s",
              bcnameid, para->bc->bcname[bcnameid]);
      ffd_log(msg, FFD_NORMAL);      
      sprintf(msg, "read_sci_input(): VX=%f, VY=%f, VX=%f, ThermalBC=%d, T/q_dot=%f, Xi=%f", 
              U, V, W, FLTMP, TMP, MASS);
      ffd_log(msg, FFD_NORMAL);

      if(SI==1) {   
        SI=0;
        if(EI>=imax) EI=EI+SI+1;
        else EI=EI+SI;
      }
      else 
        EI=EI+SI-1;

      if(SJ==1) {
        SJ=0;
        if(EJ>=jmax) EJ=EJ+SJ+1;
        else EJ=EJ+SJ;
      }
      else 
        EJ=EJ+SJ-1;

      if(SK==1) {
        SK=0;
        if(EK>=kmax) EK=EK+SK+1;
        else EK=EK+SK;
      }
      else 
        EK=EK+SK-1;

      for(ii=SI ;ii<=EI ;ii++)
        for(ij=SJ ;ij<=EJ ;ij++)
          for(ik=SK ;ik<=EK ;ik++) {
            BINDEX[0][index] = ii;
            BINDEX[1][index] = ij;
            BINDEX[2][index] = ik;
            BINDEX[3][index] = FLTMP;
            BINDEX[4][index] = bcnameid;
            index++;
            if(FLTMP==1) var[TEMPBC][IX(ii,ij,ik)] = TMP;
            if(FLTMP==0) var[QFLUXBC][IX(ii,ij,ik)] = TMP;
            flagp[IX(ii,ij,ik)] = 1; // Flag for solid
          } // End of assigning value for internal solid block
    }
  }

  /*---------------------------------------------------------------------------
  | Read the wall boundary conditions
  ----------------------------------------------------------------------------*/
  fgets(string, 400, file_params);
  sscanf(string,"%d", &para->bc->nb_wall); 
  sprintf(msg, "read_sci_input(): para->bc->nb_wall=%d", para->bc->nb_wall);
  ffd_log(msg, FFD_NORMAL);

  if(para->bc->nb_wall!=0) {
    // Read wall conditions for each wall
    for(i=1; i<=para->bc->nb_wall; i++) {
      fgets(string, 400, file_params);
      // X_index_start, Y_index_Start, Z_index_Start, 
      // X_index_End, Y_index_End, Z_index_End, 
      // Thermal Codition (0: Flux; 1:Temperature), Value of thermal conditon
      sscanf(string,"%s%d%d%d%d%d%d%d%f", &name, &SI, &SJ, &SK, &EI, 
             &EJ, &EK, &FLTMP, &TMP);
      bcnameid++;
      para->bc->bcname[bcnameid] = (char*)malloc(sizeof(name));
      strcpy(para->bc->bcname[bcnameid], name);
      sprintf(msg, "read_sci_input(): para->bc->bcname[%d]=%s",
              bcnameid, para->bc->bcname[bcnameid]);
      ffd_log(msg, FFD_NORMAL);
      sprintf(msg, "read_sci_input(): ThermalBC=%d, T/q_dot=%f", 
              FLTMP, TMP);
      ffd_log(msg, FFD_NORMAL);

      // Reset X index
      if(SI==1) {
        SI = 0;
        if(EI>=imax) EI = EI + 1;
      }
      else // 
        EI = EI + SI;
      // Reset Y index
      if(SJ==1) {
        SJ = 0;
        if(EJ>=jmax) EJ = EJ + 1;
      }
      else 
        EJ = EJ + SJ;
      // Reset Z index
      if(SK==1) {   
        SK = 0;
        if(EK>=kmax) EK = EK + 1;
      }
      else 
        EK = EK + SK;

      // Assign value for each wall cell
      for(ii=SI; ii<=EI; ii++)
        for(ij=SJ; ij<=EJ; ij++)
          for(ik=SK; ik<=EK; ik++) {
            // If cell hasn't been updated (default value -1)
            if(flagp[IX(ii,ij,ik)]<0) {
              BINDEX[0][index] = ii;
              BINDEX[1][index] = ij;
              BINDEX[2][index] = ik;
              // Define the thermal boundary property
              BINDEX[3][index] = FLTMP;
              BINDEX[4][index] = bcnameid;
              index++;  

              // Set the cell to solid
              flagp[IX(ii,ij,ik)] = 1; 
              if(FLTMP==1) var[TEMPBC][IX(ii,ij,ik)] = TMP; 
              if(FLTMP==0) var[QFLUXBC][IX(ii,ij,ik)] = TMP;
            }
          } // End of assigning value for each wall cell
    } // End of assigning value for each wall surface
  } // End of assigning value for wall boundary 

  /*---------------------------------------------------------------------------
  | Read the boundary conditions for contamiant source
  | Fixme: The data is ignored in current version
  ----------------------------------------------------------------------------*/
  fgets(string, 400, file_params);
  sscanf(string,"%d", &para->bc->nb_source); 
  sprintf(msg, "read_sci_input(): para->bc->nb_source=%d", para->bc->nb_source);
  ffd_log(msg, FFD_NORMAL);

  if(para->bc->nb_source!=0) {
    sscanf(string,"%s%d%d%d%d%d%d%f", 
           &name, &SI, &SJ, &SK, &EI, &EJ, &EK, &MASS);
    bcnameid++;
    para->bc->bcname[bcnameid] = (char*)malloc(sizeof(name));
    strcpy(para->bc->bcname[bcnameid], name);
    sprintf(msg, "read_sci_input(): para->bc->bcname[%d]=%s",
            bcnameid, para->bc->bcname[bcnameid]);
    ffd_log(msg, FFD_NORMAL);
    sprintf(msg, "read_sci_input(): Xi_dot=%f", MASS);
    ffd_log(msg, FFD_NORMAL);

    // Fixme:Need to add code to assign the BC value as other part does
  }

  para->geom->index=index;

  // Discard the unused data
  temp = fgets(string, 400, file_params); //maximum iteration
  temp = fgets(string, 400, file_params); //convergence rate
  temp = fgets(string, 400, file_params); //Turbulence model
  temp = fgets(string, 400, file_params); //initial value
  temp = fgets(string, 400, file_params); //minimum value
  temp = fgets(string, 400, file_params); //maximum value
  temp = fgets(string, 400, file_params); //fts value
  temp = fgets(string, 400, file_params); //under relaxation
  temp = fgets(string, 400, file_params); //reference point
  temp = fgets(string, 400, file_params); //monitering point

  // Read setting for restarting the old FFD simulation
  fgets(string, 400, file_params);
  sscanf(string,"%d", &para->inpu->read_old_ffd_file);
  sprintf(msg, "read_sci_input(): para->inpu->read_old_ffd_file=%d",
          para->inpu->read_old_ffd_file);
  ffd_log(msg, FFD_NORMAL);

  // Discard the unused data
  temp = fgets(string, 400, file_params); //print frequency
  temp = fgets(string, 400, file_params); //Pressure variable Y/N
  temp = fgets(string, 400, file_params); //Steady state, buoyancy.

  // Read physical properties
  fgets(string, 400, file_params);
  sscanf(string,"%f %f %f %f %f %f %f %f %f", &para->prob->rho, 
         &para->prob->nu, &para->prob->cond, 
         &para->prob->gravx, &para->prob->gravy, &para->prob->gravz, 
         &para->prob->beta, &trefmax, &para->prob->Cp);

  sprintf(msg, "read_sci_input(): para->prob->rho=%f", para->prob->rho);
  ffd_log(msg, FFD_NORMAL);

  sprintf(msg, "read_sci_input(): para->prob->nu=%f", para->prob->nu);
  ffd_log(msg, FFD_NORMAL);

  sprintf(msg, "read_sci_input(): para->prob->cond=%f", para->prob->cond);
  ffd_log(msg, FFD_NORMAL);

  sprintf(msg, "read_sci_input(): para->prob->gravx=%f", para->prob->gravx);
  ffd_log(msg, FFD_NORMAL);

  sprintf(msg, "read_sci_input(): para->prob->gravy=%f", para->prob->gravy);
  ffd_log(msg, FFD_NORMAL);

  sprintf(msg, "read_sci_input(): para->prob->gravz=%f", para->prob->gravz);
  ffd_log(msg, FFD_NORMAL);

  sprintf(msg, "read_sci_input(): para->prob->beta=%f", para->prob->beta);
  ffd_log(msg, FFD_NORMAL);

  //para->prob->trefmax=trefmax;
  sprintf(msg, "read_sci_input(): para->prob->Cp=%f", para->prob->Cp);
  ffd_log(msg, FFD_NORMAL);

  // Read simulation time settings
  fgets(string, 400, file_params);
  sscanf(string,"%f %lf %d", &para->mytime->t_start, &para->mytime->dt,
    &para->mytime->step_total);

  sprintf(msg, "read_sci_input(): para->mytime->t_start=%f", 
          para->mytime->t_start);
  ffd_log(msg, FFD_NORMAL);

  sprintf(msg, "read_sci_input(): para->mytime->dt=%f", para->mytime->dt);
  ffd_log(msg, FFD_NORMAL);

  sprintf(msg, "read_sci_input(): para->mytime->step_total=%d", 
          para->mytime->step_total);
  ffd_log(msg, FFD_NORMAL);

  temp = fgets(string, 400, file_params); //prandtl
  fclose(file_params);

  free(delx);
  free(dely);
  free(delz);
  
  sprintf(msg, "read_sci_input(): Read sci input file %s", 
          para->inpu->parameter_file_name);
  ffd_log(msg, FFD_NORMAL);
  return 0;
} // End of read_sci_input()


///////////////////////////////////////////////////////////////////////////////
/// Read the zoneone.dat file to indentify the block cells
///
///\param para Pointer to FFD parameters
///\param var Pointer to FFD simulation variables
///\param BINDEX Pointer to boundary index
///
///\return 0 if no error occurred
///////////////////////////////////////////////////////////////////////////////
int read_sci_zeroone(PARA_DATA *para, REAL **var, int **BINDEX) {
  int i,j, k;
  int delcount=0;
  int mark;
  int imax = para->geom->imax;
  int jmax = para->geom->jmax;
  int kmax = para->geom->kmax;
  int index = para->geom->index;
  int IMAX = imax+2, IJMAX = (imax+2)*(jmax+2); 
  REAL *flagp = var[FLAGP], *flagu = var[FLAGU],
       *flagv = var[FLAGV], *flagw = var[FLAGW];
  char msg[100];

  if( (file_params=fopen("zeroone.dat","r")) == NULL )
  {
    ffd_log("read_sci_input():Could not open file zeroone.dat!\n", FFD_ERROR);
    return 1;
  }

  sprintf(msg, "read_sci_input(): start to read zeroone.dat.");
  ffd_log(msg, FFD_NORMAL);

  for(k=1;k<=kmax;k++)
    for(j=1;j<=jmax;j++)
      for(i=1;i<=imax;i++) {
        fscanf(file_params,"%d" ,&mark); 

        // mark=1 block cell;mark=0 fluid cell

        if(mark==1) {
          flagp[IX(i,j,k)]=1;
          BINDEX[0][index]=i;
          BINDEX[1][index]=j;
          BINDEX[2][index]=k;
          index++;
        }
        delcount++;
        
        if(delcount==25) {
          fscanf(file_params,"\n"); 
          delcount=0; 
        }
      }

  fclose(file_params);
  para->geom->index=index;

  sprintf(msg, "read_sci_input(): end of reading zeroone.dat.");
  ffd_log(msg, FFD_NORMAL);

  return 0;
} // End of read_sci_zeroone()


///////////////////////////////////////////////////////////////////////////////
/// Identify the properties of cells
///
///\param para Pointer to FFD parameters
///\param var Pointer to FFD simulation variables
///
///\return 0 if no error occurred
///////////////////////////////////////////////////////////////////////////////
void mark_cell(PARA_DATA *para, REAL **var) {
  int i,j, k;
  int imax = para->geom->imax;
  int jmax = para->geom->jmax;
  int kmax = para->geom->kmax;
  int index = para->geom->index;
  int IMAX = imax+2, IJMAX = (imax+2)*(jmax+2); 
  REAL *flagu = var[FLAGU],*flagv = var[FLAGV],*flagw = var[FLAGW];
  REAL *flagp = var[FLAGP];

  flagp[IX(0,0,0)]=1;
  flagp[IX(0,0,kmax+1)]=1;
  flagp[IX(0,jmax+1,0)]=1;
  flagp[IX(0,jmax+1,kmax+1)]=1;
  flagp[IX(imax+1,0,0)]=1;
  flagp[IX(imax+1,0,kmax+1)]=1;
  flagp[IX(imax+1,jmax+1,0)]=1;
  flagp[IX(imax+1,jmax+1,kmax+1)]=1;

  FOR_EACH_CELL

  if(flagp[IX(i,j,k)]>=0) continue;

  if(flagp[IX(i-1,j,k)]>=0 && flagp[IX(i+1,j,k)]>=0 &&
     flagp[IX(i,j-1,k)]>=0 && flagp[IX(i,j+1,k)]>=0 &&
     flagp[IX(i,j,k-1)]>=0 && flagp[IX(i,j,k+1)]>=0 )
     flagp[IX(i,j,k)]=1;
  END_FOR

  FOR_ALL_CELL

  if(flagp[IX(i,j,k)]==1) {

    flagu[IX(i,j,k)]=1;
    flagv[IX(i,j,k)]=1;
    flagw[IX(i,j,k)]=1;

    if(i!=0) flagu[IX(i-1,j,k)]=1;
    if(j!=0) flagv[IX(i,j-1,k)]=1;
    if(k!=0) flagw[IX(i,j,k-1)]=1;
  }

  if(flagp[IX(i,j,k)]==0) {
    flagu[IX(i,j,k)]=0;
    flagv[IX(i,j,k)]=0;
    flagw[IX(i,j,k)]=0;

    if(i!=0) flagu[IX(i-1,j,k)]=0;
    if(j!=0) flagv[IX(i,j-1,k)]=0;
    if(k!=0) flagw[IX(i,j,k-1)]=0;
}

  if(flagp[IX(i,j,k)]==2) {

    flagu[IX(i,j,k)]=2;
    flagv[IX(i,j,k)]=2;
    flagw[IX(i,j,k)]=2;

    if(i!=0) flagu[IX(i-1,j,k)]=2;
    if(j!=0) flagv[IX(i,j-1,k)]=2;
    if(k!=0) flagw[IX(i,j,k-1)]=2;
}

  END_FOR
} // End of mark_cell()