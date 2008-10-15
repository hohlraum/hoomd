/*
Highly Optimized Object-Oriented Molecular Dynamics (HOOMD) Open
Source Software License
Copyright (c) 2008 Ames Laboratory Iowa State University
All rights reserved.

Redistribution and use of HOOMD, in source and binary forms, with or
without modification, are permitted, provided that the following
conditions are met:

* Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names HOOMD's
contributors may be used to endorse or promote products derived from this
software without specific prior written permission.

Disclaimer

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND
CONTRIBUTORS ``AS IS''  AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 

IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS  BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "fftw_wrapper.h"
#include <math.h>

/*! \file fftw_wrapper.cpp
	\brief Implements the fftw transform suitable for HOOMD
*/

fftw_wrapper::fftw_wrapper(){
			in_f=NULL;
			out_f=NULL;
			in_b=NULL;
			out_b=NULL;
			plan_is_defined=false;
}
fftw_wrapper::fftw_wrapper(unsigned int Nx,unsigned int Ny,unsigned int Nz):N_x(Nx),N_y(Ny),N_z(Nz)
{
		unsigned int uni_ind;
		
		in_f=(fftw_complex*) fftw_malloc(sizeof(fftw_complex)*N_x*N_y*N_z);
		out_f=(fftw_complex*) fftw_malloc(sizeof(fftw_complex)*N_x*N_y*N_z);
		in_b=(fftw_complex*) fftw_malloc(sizeof(fftw_complex)*N_x*N_y*N_z);
		out_b=(fftw_complex*) fftw_malloc(sizeof(fftw_complex)*N_x*N_y*N_z);


		for(unsigned int i=0;i<Nx;i++){
			 for(unsigned int j=0;j<Ny;j++){
				 for(unsigned int k=0;k<Nz;k++){
				 uni_ind=i+Nx*j+Nx*Ny*k;
					in_f[uni_ind][0]=exp(-static_cast<double>(i+j+k));
        			in_f[uni_ind][1]=0.0;
					in_b[i][0]=Initial_Conf_real(static_cast<double>(Nx),static_cast<double>(i))*Initial_Conf_real(static_cast<double>(Ny),static_cast<double>(j))*Initial_Conf_real(static_cast<double>(Nz),static_cast<double>(k));	
					in_b[i][1]=Initial_Conf_Imag(static_cast<double>(Nx),static_cast<double>(i))*Initial_Conf_Imag(static_cast<double>(Ny),static_cast<double>(j))*Initial_Conf_Imag(static_cast<double>(Nz),static_cast<double>(k));	
       										   } 
											}
										}
		
		p_forward=fftw_plan_dft_3d(N_x,N_y,N_z,in_f,out_f,FFTW_FORWARD,FFTW_MEASURE);
		p_backward=fftw_plan_dft_3d(N_x,N_y,N_z,in_b,out_b,FFTW_BACKWARD,FFTW_MEASURE);
		plan_is_defined=true;
			
}
fftw_wrapper::~fftw_wrapper()
{
	if(plan_is_defined){
		fftw_destroy_plan(p_forward);
		fftw_destroy_plan(p_backward);
		fftw_free(in_b);
		fftw_free(in_f);
		fftw_free(out_b);
		fftw_free(out_f);
	}

}

void fftw_wrapper::fftw_define(unsigned int Nx,unsigned int Ny,unsigned int Nz)
{
	unsigned int uni_ind;

	if(!plan_is_defined){
		N_x=Nx;
		N_y=Ny;
		N_z=Nz;

		in_f=(fftw_complex*) fftw_malloc(sizeof(fftw_complex)*N_x*N_y*N_z);
		out_f=(fftw_complex*) fftw_malloc(sizeof(fftw_complex)*N_x*N_y*N_z);
		in_b=(fftw_complex*) fftw_malloc(sizeof(fftw_complex)*N_x*N_y*N_z);
		out_b=(fftw_complex*) fftw_malloc(sizeof(fftw_complex)*N_x*N_y*N_z);

		for(unsigned int i=0;i<Nx;i++){
			 for(unsigned int j=0;j<Ny;j++){
				 for(unsigned int k=0;k<Nz;k++){
				 uni_ind=i+Nx*j+Nx*Ny*k;
					in_f[uni_ind][0]=exp(-static_cast<double>(i+j+k));
        			in_f[uni_ind][1]=0.0;
					in_b[uni_ind][0]=Initial_Conf_real(static_cast<double>(Nx),static_cast<double>(i))*Initial_Conf_real(static_cast<double>(Ny),static_cast<double>(j))*Initial_Conf_real(static_cast<double>(Nz),static_cast<double>(k));	
					in_b[uni_ind][1]=Initial_Conf_Imag(static_cast<double>(Nx),static_cast<double>(i))*Initial_Conf_Imag(static_cast<double>(Ny),static_cast<double>(j))*Initial_Conf_Imag(static_cast<double>(Nz),static_cast<double>(k));	
       										   } 
											}
										}


		p_forward=fftw_plan_dft_3d(N_x,N_y,N_z,in_f,out_f,FFTW_FORWARD,FFTW_ESTIMATE);
		p_backward=fftw_plan_dft_3d(N_x,N_y,N_z,in_b,out_b,FFTW_BACKWARD,FFTW_ESTIMATE);
		plan_is_defined=true;
	}
}

void fftw_wrapper::cmplx_fft(unsigned int Nx,unsigned int Ny,unsigned int Nz,CScalar ***Dat_in,CScalar ***Dat_out,int sig)
{
	int uni_ind=0;
	if(sig>0){
     for(unsigned int i=0;i<Nx;i++){
		 for(unsigned int j=0;j<Ny;j++){
			 for(unsigned int k=0;k<Nz;k++){
				 uni_ind=i+Nx*j+Nx*Ny*k;
			 in_f[uni_ind][0]=static_cast<double>((Dat_in[i][j][k]).r);
        	 in_f[uni_ind][1]=static_cast<double>((Dat_in[i][j][k]).i);
       			} 
		 }
	 }
		fftw_execute(p_forward);
	  for(unsigned int i=0;i<Nx;i++){
		 for(unsigned int j=0;j<Ny;j++){
			 for(unsigned int k=0;k<Nz;k++){
				 uni_ind=i+Nx*j+Nx*Ny*k;
			(Dat_out[i][j][k]).r=static_cast<Scalar>(out_f[uni_ind][0]);
        	(Dat_out[i][j][k]).i=static_cast<Scalar>(out_f[uni_ind][1]);
       								} 
								}
							}
		 }
	else{
		 for(unsigned int i=0;i<Nx;i++){
		 for(unsigned int j=0;j<Ny;j++){
			 for(unsigned int k=0;k<Nz;k++){
				 uni_ind=i+Nx*j+Nx*Ny*k;
			 in_b[uni_ind][0]=static_cast<double>((Dat_in[i][j][k]).r);
        	 in_b[uni_ind][1]=static_cast<double>((Dat_in[i][j][k]).i);
       			} 
		 }
		 }
		fftw_execute(p_backward);
	  for(unsigned int i=0;i<Nx;i++){
		 for(unsigned int j=0;j<Ny;j++){
			 for(unsigned int k=0;k<Nz;k++){
				 uni_ind=i+Nx*j+Nx*Ny*k;
			(Dat_out[i][j][k]).r=static_cast<Scalar>(out_b[uni_ind][0]);
        	(Dat_out[i][j][k]).i=static_cast<Scalar>(out_b[uni_ind][1]);
       								} 
								}
							}

			}
}
void fftw_wrapper::real_to_compl_fft(unsigned int Nx,unsigned int Ny,unsigned int Nz,Scalar ***Data_in,CScalar ***Data_out)
{
//TO DO
}

void fftw_wrapper::compl_to_real_fft(unsigned int Nx,unsigned int Ny,unsigned int Nz,CScalar ***Data_in,Scalar ***Data_out)
{
//TO DO
}
double fftw_wrapper::Initial_Conf_real(double x,double y)
{
  return (1-exp(-x))*(1-exp(-1.0)*cos(2*M_PI*y/x))/(1+exp(-2.0)-2*exp(-1.0)*cos(2*M_PI*y/x));	
}
double fftw_wrapper::Initial_Conf_Imag(double x,double y)
{
  return -(1-exp(-x))*exp(-1.0)*sin(2*M_PI*y/x)/(1+exp(-2.0)-2*exp(-1.0)*cos(2*M_PI*y/x));	
}