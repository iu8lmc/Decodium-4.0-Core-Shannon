subroutine fspread_lorentz(cdat,fspread)

  parameter (NZ=3*12000)
  complex cdat(0:NZ-1)
  complex cspread(0:NZ-1)
  complex z

  twopi=8.0*atan(1.0)
  nfft=NZ
  nh=nfft/2
  df=12000.0/nfft
  cspread(0)=1.0
  cspread(nh)=0.
  b=6.0                       !Use truncated Lorenzian shape for fspread
  do i=1,nh
     f=i*df
     x=b*f/fspread
     z=0.
     a=0.
     if(x.lt.3.0) then                          !Cutoff beyond x=3
        a=sqrt(1.111/(1.0+x*x)-0.1)             !Lorentzian amplitude
        phi1=twopi*rran()                       !Random phase
        z=a*cmplx(cos(phi1),sin(phi1))
     endif
     cspread(i)=z
     z=0.
     if(x.lt.3.0) then                !Same thing for negative freqs
        phi2=twopi*rran()
        z=a*cmplx(cos(phi2),sin(phi2))
     endif
     cspread(nfft-i)=z
  enddo

  call four2a(cspread,nfft,1,1,1)             !Transform to time domain
  
  sum=0.
  do i=0,nfft-1
     p=real(cspread(i))**2 + aimag(cspread(i))**2
     sum=sum+p
  enddo
  avep=sum/nfft
  fac=sqrt(1.0/avep)
  cspread=fac*cspread                   !Normalize to constant avg power
  cdat=cspread*cdat                     !Apply Rayleigh fading

  return
end subroutine fspread_lorentz
