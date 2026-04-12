subroutine degrade_snr(d2,npts,db,bw)

! Degrade S/N by specified number of dB.

  integer*2 d2(npts)
  
  p0=0.
  do i=1,npts
     x=d2(i)
     p0=p0 + x*x
  enddo
  p0=p0/npts
  if(bw.gt.0.0) p0=p0*6000.0/bw
  s=sqrt(p0*(10.0**(0.1*db) - 1.0))
  fac=sqrt(p0/(p0+s*s))
  do i=1,npts
     d2(i)=nint(fac*(d2(i) + s*gran()))
  enddo

  return
end subroutine degrade_snr
