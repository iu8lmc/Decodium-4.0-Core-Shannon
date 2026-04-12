subroutine timf2(k,nfft,nwindow,nb,peaklimit,faclim, &
  cx0,cx1,slimit,lstrong,px,nzap)

! Sequential processing of time-domain I/Q data, using Linrad-like
! "first FFT" and "first backward FFT".  

!  cx0       - complex input data
!  nfft      - length of FFTs
!  nwindow   - 0 for no window, 2 for sin^2 window
!  cx1       - output data

! Non-windowed processing means no overlap, so kstep=nfft.  
! Sin^2 window has 50% overlap, kstep=nfft/2.

! Frequencies with strong signals are identified and separated.  The back
! transforms are done separately for weak and strong signals, so that
! noise blanking can be applied to the weak-signal portion.  Strong and
! weak are finally re-combined in the time domain.

  parameter (MAXFFT=1024,MAXNH=MAXFFT/2)
  parameter (MAXSIGS=100)
  complex cx0(0:nfft-1),cx1(0:nfft-1)
  complex cx(0:MAXFFT-1),cxt(0:MAXFFT-1)
  complex cxs(0:MAXFFT-1),covxs(0:MAXNH-1)     !Strong X signals
  complex cxw(0:MAXFFT-1),covxw(0:MAXNH-1)     !Weak X signals
  real*4 w(0:MAXFFT-1)
  real*4 s(0:MAXFFT-1)
  logical*1 lstrong(0:MAXFFT-1),lprev
  integer ia(MAXSIGS),ib(MAXSIGS)
  logical first
  data first/.true./
  data k0/99999999/
  save

  if(faclim.eq.-9999.0) stop   !Silence compiler warning.
  if(first) then
     pi=4.0*atan(1.0)
     do i=0,nfft-1
        w(i)=(sin(i*pi/nfft))**2
     enddo
     s=0.
     nh=nfft/2
     kstep=nfft
     if(nwindow.eq.2) kstep=nh
     fac=1.0/nfft
     slimit=1.e30
     first=.false.
  endif

  if(k.lt.k0) then
     covxs=0.
     covxw=0.
     covys=0.
     covyw=0.
  endif
  k0=k

  cx(0:nfft-1)=cx0
  if(nwindow.eq.2) cx(0:nfft-1)=w(0:nfft-1)*cx(0:nfft-1)
  call four2a(cx,nfft,1,1,1)                       !First forward FFT (X)
  cxt(0:nfft-1)=cx(0:nfft-1)

! Identify frequencies with strong signals, copy frequency-domain
! data into array cs (strong) or cw (weak).

  do i=0,nfft-1
     p=real(cxt(i))**2 + aimag(cxt(i))**2
     s(i)=p
  enddo
  ave=sum(s(0:nfft-1))/nfft
  lstrong(0:nfft-1)=s(0:nfft-1).gt.10.0*ave

  nsigs=0
  lprev=.false.
  iwid=1
  ib=-99
  do i=0,nfft-1
     if(lstrong(i) .and. (.not.lprev)) then
        if(nsigs.lt.MAXSIGS) nsigs=nsigs+1
        ia(nsigs)=i-iwid
        if(ia(nsigs).lt.0) ia(nsigs)=0
     endif
     if(.not.lstrong(i) .and. lprev) then
        ib(nsigs)=i-1+iwid
        if(ib(nsigs).gt.nfft-1) ib(nsigs)=nfft-1
     endif
     lprev=lstrong(i)
  enddo

  if(nsigs.gt.0) then
     do i=1,nsigs
        ja=ia(i)
        jb=ib(i)
        if(ja.lt.0 .or. ja.gt.nfft-1 .or. jb.lt.0 .or. jb.gt.nfft-1) then
           cycle
        endif
        if(jb.eq.-99) jb=ja + min(2*iwid,nfft-1)
        lstrong(ja:jb)=.true.
     enddo
  endif

  do i=0,nfft-1
     if(lstrong(i)) then
        cxs(i)=fac*cxt(i)
        cxw(i)=0.
     else
        cxw(i)=fac*cxt(i)
        cxs(i)=0.
     endif
  enddo

  call four2a(cxw,nfft,1,-1,1)                 !Transform weak and strong X
  call four2a(cxs,nfft,1,-1,1)                 !back to time domain, separately

  if(nwindow.eq.2) then
     cxw(0:nh-1)=cxw(0:nh-1)+covxw(0:nh-1)     !Add previous segment's 2nd half
     covxw(0:nh-1)=cxw(nh:nfft-1)              !Save 2nd half
     cxs(0:nh-1)=cxs(0:nh-1)+covxs(0:nh-1)     !Ditto for strong signals
     covxs(0:nh-1)=cxs(nh:nfft-1)
  endif

! Apply noise blanking to weak data
  if(nb.ne.0) then
     do i=0,kstep-1
        peak=abs(cxw(i))
        if(peak.gt.peaklimit) then
           cxw(i)=0.
           nzap=nzap+1
        endif
     enddo
  endif

! Compute power levels from weak data only
  do i=0,kstep-1
     px=px + real(cxw(i))**2 + aimag(cxw(i))**2
  enddo
  cx1(0:kstep-1)=cxw(0:kstep-1) + cxs(0:kstep-1)       !Weak + strong (X)

  return
end subroutine timf2
