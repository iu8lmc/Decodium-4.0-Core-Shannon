subroutine makepings(pings,nTRperiod,npts,width,sig)

  real pings(npts)
  real*8 t
  real t0(29)

  dt=1.0/12000.0
  do i=1,nTRperiod-1
     t0(i)=i                            !Make pings at t=1, 2, ... 14 s.
  enddo
  amp=sig

  do i=1,npts
     iping=min(max(1,i/12000),nTRperiod-1)
     t=(i*dt-t0(iping))/width
     if(t.lt.0.d0 .or. t.gt.10.0) then
        fac=0.
     else
        fac=2.718*t*dexp(-t)
     endif
     pings(i)=fac*amp
  enddo

  return
end subroutine makepings
