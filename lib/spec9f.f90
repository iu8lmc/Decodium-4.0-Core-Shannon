subroutine spec9f(id2,npts,nsps,s1,jz,nq)

! Compute symbol spectra at quarter-symbol steps.

  integer*2 id2(0:npts)
  real s1(nq,jz)
  real x(960)
  complex c(0:480)
  equivalence (x,c)

  nfft=2*nsps                               !FFTs at twice the symbol length
  nh=nfft/2
  do j=1,jz
     ia=(j-1)*nsps/4
     ib=ia+nsps-1
     if(ib.gt.npts) exit
     x(1:nh)=id2(ia:ib)
     x(nh+1:)=0.
     call four2a(x,nfft,1,-1,0)           !r2c
     k=mod(j-1,340)+1
     do i=1,NQ
        s1(i,j)=1.e-10*(real(c(i))**2 + aimag(c(i))**2)
     enddo
  enddo

!### Reference spectrum should be applied here (or possibly earlier?) ###
!### Normalize so that rms (or level?) is 1.0 ?  ###

  return
end subroutine spec9f
